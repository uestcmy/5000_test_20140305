// Copyright (c) 2011 Alcatel-Lucent Shanghai Bell. All rights reserved.
// Author:  WANG Yang, yang.h.wang@alcatel-sbell.com.cn
//
// ML605 API - for GPP demo
//
// Notes:
//  - This API support user applications with ML605 PCIe card features.
//  - Must be used together with ML605 raw data driver and xdma driver.
//
// Revision history:
//  v1.00, October 31, 2011
//    - First release.

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#include "xpmon_be.h"
#include "ml605_api.h"

#define RAWDATA_FILENAME    "/dev/ml605_raw_data"
#define XDMA_FILENAME       "/dev/xdma_stat"
#define PKTSIZE             4096

/* File pointer for the raw data file */
static int rawdatafd = -1;

/* File pointer for the xdma status file */
static int xdmadatafd = -1;

static const int kTimeOut = 1000;

static const int kSleepUs = 5;

int ML605Open() {
  if ((xdmadatafd = open(XDMA_FILENAME, O_RDONLY)) < 0) {
    printf("Failed open %s\n", XDMA_FILENAME);
    return xdmadatafd;
  }

  if ((rawdatafd = open(RAWDATA_FILENAME, O_RDWR)) < 0) {
    printf("Failed open %s\n", RAWDATA_FILENAME);
    close(xdmadatafd);
  }

	// wait until raw data driver is ready
	sleep(1);

  return rawdatafd;
}

int ML605Close(int fd) {
  int retval;

  if (fd != rawdatafd) {
    printf("Close: wrong fd\n");
    return -EBADF;
  }

  if ((retval = close(rawdatafd)) < 0) {
    printf("Failed close %s\n", RAWDATA_FILENAME);
    return retval;
  }

  if ((retval = close(xdmadatafd)) < 0) {
    printf("Failed close %s\n", XDMA_FILENAME);
    return retval;
  }

  return 0;
}

#if 0
int ML605StartEthernet(int fd) {
  TestCmd testCmd;
  int retval;

  if (fd != rawdatafd) {
    printf("Start Ethernet: wrong fd\n");
    return -EBADF;
  }

  // wait until FPGA is ready
  sleep(1);

  testCmd.Engine = 1;
  testCmd.TestMode = 0;
  testCmd.MinPktSize = testCmd.MaxPktSize = PKTSIZE;

	// clear the Ethernet Tx start bit
  retval = ioctl(xdmadatafd, ISTOP_TEST, &testCmd);
  if(retval != 0) {
    printf("Clear StartEthernet on Eng %d failed\n", testCmd.Engine);
    return retval;
  }
  testCmd.TestMode = TEST_START | ENABLE_LOOPBACK;
  // set the Ethernet Tx start bit
  retval = ioctl(xdmadatafd, ISTART_TEST, &testCmd);
  if(retval != 0) {
    printf("Set StartEthernet on Eng %d failed\n", testCmd.Engine);
    return retval;
  }

  return 0;
}
#else
int ML605StartEthernet(int fd, int flag) {
  TestCmd testCmd;
  int retval;

  if (fd != rawdatafd) {
    printf("Start Ethernet: wrong fd\n");
    return -EBADF;
  }

//  testCmd.Engine = 1;
//  testCmd.TestMode = 0;
//  testCmd.MinPktSize = testCmd.MaxPktSize = PKTSIZE;

  testCmd.Engine = 1;
  
  switch (flag) {
  case SFP_TX_START:
    // get current SFP state
    retval = ioctl(xdmadatafd, IGET_TEST_STATE, &testCmd);
    if(retval != 0) {
      printf("ML605StartEthernet(): Get SFP state of Eng %d failed\n", testCmd.Engine);
      return retval;
    }
    // check whether SFP Tx is already running
    if (testCmd.TestMode & ENABLE_LOOPBACK) {
      printf("ML605StartEthernet(): Restart SFP Tx is not allowed. Close both Tx and Rx program, then remove and re-insert PCIe driver.\n");
      return -1;
    }
    // start SFP Tx -> set ENABLE_LOOPBACK (TX_EN) bit
    testCmd.TestMode = testCmd.TestMode | TEST_START | ENABLE_LOOPBACK;
    retval = ioctl(xdmadatafd, ISTART_TEST, &testCmd);
    if(retval != 0) {
      printf("Start SFP Tx of Eng %d failed\n", testCmd.Engine);
      return retval;
    }
    break;
  case SFP_RX_START:
    // get current SFP state
    retval = ioctl(xdmadatafd, IGET_TEST_STATE, &testCmd);
    if(retval != 0) {
      printf("Get SFP state of Eng %d failed\n", testCmd.Engine);
      return retval;
    }
    // check whether SFP Rx is already running
    if (testCmd.TestMode & ENABLE_PKTCHK) {
      printf("ML605StartEthernet(): Restart SFP Rx is not allowed. Close both Tx and Rx program, then remove and re-insert PCIe driver.\n");
      return -1;
    }
    // start SFP Rx -> set ENABLE_PKTCHK (RX_EN) bit
    testCmd.TestMode = testCmd.TestMode | TEST_START | ENABLE_PKTCHK;
    retval = ioctl(xdmadatafd, ISTART_TEST, &testCmd);
    if(retval != 0) {
      printf("Start SFP Rx of Eng %d failed\n", testCmd.Engine);
      return retval;
    }
    break;
  default:
    break;
  }

  return 0;
}
#endif

int ML605Send(int fd, const void *buf, unsigned int len) {
  int num_wait;
  int num_pkts;
  int bytes;
  int retval;
  int i;
  int busy_counter = 0;

  if (fd != rawdatafd) {
    printf("Send: wrong fd\n");
    return -EBADF;
  }

  if ((len <= 0) || (len > 1024*1024) || ((len & 0x00000FFF) != 0)) {
    printf("Send: Invalid packet length %d. Must be less than 1 MB. Must be multiples of 4096.\n", len);
    return -EINVAL;
  }

  num_wait = 0;
  while (ML605QueryTxBuf(fd) < static_cast<int>(len)*2) {
    usleep(1000);
    ++num_wait;
    if (num_wait > kTimeOut) {
      printf("ML605Send timeout > %d ms\n", 1000*kTimeOut/1000);
      return -EFAULT;
    }
  }

//	printf("TxFreeBuf=%d\n", ML605QueryTxBuf(fd));

  num_pkts = len >> 12;
  bytes = retval = 0;
  for (i = 0; i < num_pkts; ++i)
  {
//		printf("i=%d\n", i);
    while (busy_counter < kTimeOut) {
      bytes = write(rawdatafd, reinterpret_cast<const unsigned char*>(buf)+i*PKTSIZE, PKTSIZE);
		  if (bytes == PKTSIZE) {
		    break;            // send sucessfully, goto next loop
		  } else if (bytes >= 0) {
	      printf("ML605Send: partially send bytes=%d\n", bytes);
	      //return -EFAULT;   // partially sent, return with error
	    } else if (errno == EBUSY) {
        ++busy_counter;   // device busy, try again. Return if busy counter hits.
        if (busy_counter >= kTimeOut) {
          printf("ML605Send: device busy\n");
          return -EBUSY;
        }
	    } else {
        printf("ML605Send: errno=%d\n", errno);
        return -errno;    // Other errors, return
	    }
	    usleep(kSleepUs);
		}
		retval += bytes;
	}

	return retval;
}

int ML605QueryTxBuf(int fd) {
  int num_tx_buf_len = 0;   // available length
  int ioctl_retval;
  int busy_counter = 0;
  int retval = -EFAULT;

  if (fd != rawdatafd) {
    printf("Query Tx Buf: wrong fd\n");
    return -EBADF;
  }

  while (busy_counter < kTimeOut) {
    ioctl_retval = ioctl(fd, RD_CMD_QUERY_TX_BUF, &num_tx_buf_len);
    if (ioctl_retval == 0) {
      retval = num_tx_buf_len;    // operation successfully, return
      break;
    } else if (errno == EBUSY) {
      ++busy_counter;       // device busy, try again. Return if busy counter hits.
      if (busy_counter >= kTimeOut) {
        retval = -errno;
        printf("ML605QueryTxBuf failed: device is busy\n");
      }
    } else {
      retval = -errno;      // Other errors, return
      printf("ML605QueryTxBuf failed: errno=%d\n", errno);
      break;
    }
    usleep(kSleepUs);
  }

  return retval;
}

int ML605Recv(int fd, void *buf, unsigned int len) {
  int num_wait;
  int num_pkts;
  int bytes;
  int retval;
  int i;
  int busy_counter = 0;  

  if (fd != rawdatafd) {
    printf("Recv: wrong fd\n");
    return -EBADF;
  }

  if ((len <= 0) || (len > 1024*1024) || ((len & 0x00000FFF) != 0)) {
    printf("Recv: Invalid packet length %d. Must be less than 1 MB. Must be multiples of 4096.\n", len);
    return -EINVAL;
  }

  num_wait = 0;
  while (ML605QueryRxBuf(fd) < static_cast<int>(len)) {
    usleep(1000);
    ++num_wait;   // timeout feature might be required in future releases
    if (num_wait > kTimeOut) {
      printf("ML605Recv timeout > %d ms\n", 1000*kTimeOut/1000);
      return -EFAULT;
    }
  }

  num_pkts = len >> 12;
  bytes = retval = 0;
//	printf("num_pkts=%d\n", num_pkts);
  for (i = 0; i < num_pkts; ++i)
  {
   // printf("read times=%d\n",i);
//		printf("buf addr=0x%lx\n", (unsigned long)(buf+i*PKTSIZE));
    while (busy_counter < kTimeOut) {
      bytes = read(rawdatafd, reinterpret_cast<unsigned char*>(buf)+i*PKTSIZE, PKTSIZE);
		  if (bytes == PKTSIZE) {
		    break;            // send sucessfully, goto next loop
		  } else if (bytes >= 0) {
	      printf("ML605Recv: partially receive bytes=%d\n", bytes);
	      return -EFAULT;   // partially receive, return with error
	    } else if (errno == EBUSY) {
        ++busy_counter;   // device busy, try again. Return if busy counter hits.
        if (busy_counter >= kTimeOut) {
          printf("ML605Recv: device busy\n");
          return -EBUSY;
        }
	    } else {
        printf("ML605Recv: errno=%d\n", errno);
        return -errno;    // Other errors, return
	    }
	    usleep(kSleepUs);
		}		
		retval += bytes;
	}

	return retval;
}

int ML605QueryRxBuf(int fd) {
  int num_rx_buf_len;   // available length
  int ioctl_retval;
  int busy_counter = 0;
  int retval = -EFAULT;

  if (fd != rawdatafd) {
    printf("Query Rx Buf: wrong fd\n");
    return -EBADF;
  }

  while (busy_counter < kTimeOut) {
    ioctl_retval = ioctl(fd, RD_CMD_QUERY_RX_BUF, &num_rx_buf_len);
    if (ioctl_retval == 0) {
      retval = num_rx_buf_len;    // operation successfully, return
      break;
    } else if (errno == EBUSY) {
      ++busy_counter;        // device busy, try again. Return if busy counter hits.
      if (busy_counter >= kTimeOut) {
        retval = -errno;
        printf("ML605QueryRxBuf failed: device is busy\n");
      }
    } else {
      retval = -errno;    // Other errors, return
      printf("ML605QueryRxBuf failed: errno=%d\n", errno);
      break;
    }
    usleep(kSleepUs);    
  }
  
  return retval;
}

int ML605GetHwCounterMs(int fd) {
  int num_ms;
  int retval = -EFAULT;
  int ioctl_retval;
  int busy_counter = 0;

  if (fd != rawdatafd) {
    printf("Get HW counter: wrong fd\n");
    return -EBADF;
  }
  
  while (busy_counter < kTimeOut) {
    ioctl_retval = ioctl(fd, RD_CMD_GET_COUNTER, &num_ms);
    if (ioctl_retval == 0) {
      retval = num_ms >> 16;    // operation successfully, return
      break;
    } else if (errno == EBUSY) {
      ++busy_counter;        // device busy, try again. Return if busy counter hits.
      if (busy_counter >= kTimeOut) {
        retval = -errno;
        printf("ML605GetHwCounterMs failed: device is busy\n");
      }
    } else {
      retval = -errno;    // Other errors, return
      printf("ML605GetHwCounterMs failed: errno=%d\n", errno);
      break;
    }
    usleep(kSleepUs);
  }  

  return retval;
}

int ML605GetHwCounters(int fd, int *ptr_counter_ms, int *ptr_counter_50mhz) {
  int num_ms;
  int retval = -EFAULT;
  int busy_counter = 0;
  int ioctl_retval;

  if (fd != rawdatafd) {
    printf("Get HW counter: wrong fd\n");
    return -EBADF;
  }

  while (busy_counter < kTimeOut) {
    ioctl_retval = ioctl(fd, RD_CMD_GET_COUNTER, &num_ms);
    if (ioctl_retval == 0) {
      *ptr_counter_ms = num_ms >> 16;
      *ptr_counter_50mhz = num_ms & 0x0000FFFF;
      retval = 0;    // operation successfully, return
      break;
    } else if (errno == EBUSY) {
      ++busy_counter;        // device busy, try again. Return if busy counter hits.
      if (busy_counter >= kTimeOut) {
        retval = -errno;
        printf("ML605GetHwCounters failed: device is busy\n");
      }
    } else {
      retval = -errno;    // Other errors, return
      printf("ML605GetHwCounters failed: errno=%d\n", errno);
      break;
    }
    usleep(kSleepUs);
  }  

  return retval;
}

int ML605SetRfCmd(int fd, int rf_cmd) {
  int retval = -EFAULT;  
  int busy_counter = 0;
  int ioctl_retval;
  
  if (fd != rawdatafd) {
    printf("Set RF command: wrong fd\n");
    return -EBADF;
  }

  while (busy_counter < kTimeOut) {
    ioctl_retval = ioctl(fd, RD_CMD_SET_RF_CMD, &rf_cmd);
    if (ioctl_retval == 0) {
      retval = 0;    // operation successfully, return
      break;
    } else if (errno == EBUSY) {
      ++busy_counter;        // device busy, try again. Return if busy counter hits.
      if (busy_counter >= kTimeOut) {
        retval = -errno;
        printf("ML605SetRfCmd (0x%x) failed: device is busy\n", rf_cmd);
      }
    } else {
      retval = -errno;    // Other errors, return
      printf("ML605SetRfCmd (0x%x) failed: errno=%d\n", rf_cmd, errno);
      break;
    }
    usleep(kSleepUs);
  }  
  
  return retval;
}
