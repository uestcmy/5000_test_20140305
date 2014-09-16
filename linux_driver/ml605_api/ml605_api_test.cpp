#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <math.h>
#include <sys/resource.h>

#include "ml605_api.h"

//#define EAGLE_RF
#define SEU_RF

//#define RX_VERIFY
//#define WRITE_ONLY
#define READ_ONLY
//#define SFP_TEST

#define SRC_FILENAME        "./src.dat"
#define DST_FILENAME        "./dst.dat"
#define OFDM_SRC_FILENAME   "./tsig_test2.bin"
#define LOG_FILENAME				"./log.dat"

static int isFirstTime = true;

static const int kDelay = 3;
//static const int kTestTimes = kDelay+2;		// 1 ms delay
static const int kTestTimes = 10;           // for file test, should be multiple of 4
//static const int kTestLen = 1024*1024;
static const int kTestLen = 4096*30;    // 1 ms data
static const int kAntNum = 2;

static int fd605 = -1;

struct Complex16 {
  int16_t n16_real;
  int16_t n16_imag;
};


void SpeedTest() {
  int i, j;
	struct timeval tv_start, tv_finish;
	double dou_time;
  char ch_any;

	unsigned char testBuf[kTestLen];
	unsigned char readbackBuf[kTestLen];

#ifdef RX_VERIFY
  int num_error;
#endif

  for (i = 0; i < kTestLen; ++i) {
    testBuf[i] = i;
		readbackBuf[i] = i;
  }

  int bytes = 0;

#ifndef NO_WRITE
	// write test
	gettimeofday(&tv_start, NULL);
	for (j = 0; j < kTestTimes; ++j)
  {
    if ((bytes = ML605Send(fd605, testBuf, kTestLen)) < kTestLen) {
      printf("Send failed\n");
      return;
    }
	}
	gettimeofday(&tv_finish, NULL);
	dou_time =  tv_finish.tv_sec
						+ tv_finish.tv_usec/1000000.0
						- tv_start.tv_sec
						- tv_start.tv_usec/1000000.0;
	printf("Write completed. Elapsed time: %f seconds.\n", dou_time);
	printf("Write speed = %f Gbps\n", kTestTimes*kTestLen*8.0/dou_time/1.0e9);

//	return 0;
#endif

  printf("Press any key for Rx test\n");
  scanf("%c", &ch_any);
//	  usleep(1000);

	// read test
	gettimeofday(&tv_start, NULL);
	for (j = 0; j < kTestTimes; ++j)
	{
//		printf("readbackBuf=0x%lx\n", readbackBuf);
    if ((bytes = ML605Recv(fd605, readbackBuf, kTestLen)) < kTestLen) {
      printf("Recv failed\n");
      return;
    }

#ifdef RX_VERIFY
		num_error = 0;
//		printf("Read back values:\n");
		for (i = 0; i < kTestLen; ++i) {
	//    printf("%d\t", readbackBuf[i]);
	//    if (15 == i%16) {
	//      printf("\n");
	//    }
		  if (readbackBuf[i] != testBuf[i]) {
		    ++num_error;
		  }
			readbackBuf[i] = 0;
		}

		if (num_error) {
		  printf("Error bytes = %d\n", num_error);
		} else {
		  printf("========== Correct result %d ===========\n", j);
		}
#endif
  }
	gettimeofday(&tv_finish, NULL);
	dou_time =  tv_finish.tv_sec
						+ tv_finish.tv_usec/1000000.0
						- tv_start.tv_sec
						- tv_start.tv_usec/1000000.0;
	printf("Read completed. Elapsed time: %f seconds.\n", dou_time);
	printf("Read speed = %f Gbps\n", kTestTimes*kTestLen*8.0/dou_time/1.0e9);

	printf("=============================================\n");
	printf("Test size = %d\n", kTestLen);
	printf("Test loop = %d\n", kTestTimes);
}

unsigned int testBuf[kTestLen/4];
unsigned int readbackBuf[kTestLen/4];

void InfiniteWrite() {
  int bytes = 0;
	int i;
	int counter_ms, counter_50mhz;
	int temp_ms;
	int *ptr32_buffer = (int*)testBuf;
	int prev_ms = 0;
	int diff_ms = 0;
  int retval;
	int log_fd;
//	struct timeval tv_start, tv_finish;
//	double dou_time;
	int arr_interval_stats[10];	// statistics of PCIe sending interval, times within 1 ms, 2 ms..., 9 ms, and > 9 ms
	int interval_counter = 0;
  int tx_frames;

  if ((log_fd = open(LOG_FILENAME, O_WRONLY|O_CREAT|O_TRUNC,
       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    printf("Failed open %s\n", LOG_FILENAME);
    return;
  }

	for (i = 0; i < 10; ++i) {
		arr_interval_stats[i] = 0;
	}

	// write 400 ms data before start Tx
	for (i = 0; i < kTestTimes; ++i) {
		if (ML605GetHwCounters(fd605, &counter_ms, &counter_50mhz) < 0) {
		  printf("GetHwCounters failed\n");
		  return;
		}
		*ptr32_buffer = counter_ms << 2;

    if ((bytes = ML605Send(fd605, testBuf, kTestLen)) < kTestLen) {
      printf("Send failed. loop %d\n", i);
      return;
    }
	}

	printf("okay\n");

	while (ML605GetHwCounterMs(fd605) != 950) {
		;
	}

	if ((retval = ML605StartEthernet(fd605, SFP_TX_START)) < 0) {
		printf("Set loopback bit failed. Return %d\n", retval);
		return;
	}

	while (ML605GetHwCounterMs(fd605) >= 900) {
		;
	}

	// write test
	i = 0;
//	gettimeofday(&tv_start, NULL);
	while(1) {
		ML605GetHwCounters(fd605, &counter_ms, &counter_50mhz);
		// in order to avoid counter glitch
		while (i < 5) {
			ML605GetHwCounters(fd605, &temp_ms, &counter_50mhz);
			if (temp_ms == counter_ms) {
				++i;
			} else {
				i = 0;
				counter_ms = temp_ms;
			}
		}

    printf("%d\t%d\n", counter_ms, counter_50mhz);

		diff_ms = counter_ms - prev_ms;

		if (diff_ms < 0) {
			diff_ms += 1000;
		}

		if (diff_ms > 0) {
			if (diff_ms > 9) {
				++arr_interval_stats[9];
			} else {
				++arr_interval_stats[diff_ms-1];
			}
			++interval_counter;

			// test for 5,000 seconds (5.0e6 sub-frames)
			if (interval_counter >= 5000*1000) {
				break;
			}

//			gettimeofday(&tv_finish, NULL);
//			dou_time =  tv_finish.tv_sec
//								+ tv_finish.tv_usec/1000000.0
//								- tv_start.tv_sec
//								- tv_start.tv_usec/1000000.0;
//			printf("%d %d %d %f\n", diff_ms, counter_ms, prev_ms, dou_time*1000.0);
//			gettimeofday(&tv_start, NULL);
			if (diff_ms > 10) {
				printf("diff_ms > 10, too many running processes?\n");
				return;
			}
//		  if (write(log_fd, &diff_ms, 4) < 4) {
//		    printf("Write destination file %s error\n", LOG_FILENAME);
//		  }
		}

		prev_ms = counter_ms;

    // If we want 1 ms delay, then we can only write 1 sub-frame each time, even if the interval is longer than 1 ms.
    // Otherwise, we occupies more buffer and thus higher delay.
    tx_frames = (diff_ms > kDelay) ? kDelay : diff_ms;

		for (i = 0; i < tx_frames; ++i) {
			*ptr32_buffer = counter_ms << 2;
//		  if (write(log_fd, &counter_ms, 4) < 4) {
//		    printf("Write destination file %s error\n", LOG_FILENAME);
//		  }

		  if ((bytes = ML605Send(fd605, testBuf, kTestLen)) < kTestLen) {
		    printf("Send failed\n");
		    return;
		  }
		}
//		printf("%d\n", i++);
	}

	// print test results
	printf("Total Tx intervals = %d\nInterval statistics (times, percentage):\n", interval_counter);
	for (i = 0; i < 9; ++i) {
		printf("%d ms: %d, %e\n", i+1, arr_interval_stats[i], (double)(arr_interval_stats[i])/(double)(interval_counter));
	}
	printf("> 9 ms: %d, %e\n", arr_interval_stats[9], (double)(arr_interval_stats[9])/(double)(interval_counter));
}

void InfiniteRead() {
  int i, j;
  int dst_fd;
//  int *ptr32_buffer = (int*)readbackBuf;
//  int counter_ms;
//  int delay_tx2rx;
	int arr_delay[10];	// statistics of Tx-to-Rx delay, times within 1 ms, 2 ms..., 9 ms, and > 9 ms
	int frame_counter = 0;

  for (j = 0; j < 10; ++j) {
    arr_delay[j] = 0;
  }

  if ((dst_fd = open(DST_FILENAME, O_WRONLY|O_CREAT|O_TRUNC,
       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    printf("Failed open %s\n", DST_FILENAME);
    return;
  }

#ifdef RX_VERIFY
  int num_error;
#endif

  int bytes = 0;

	// read test
  int retval;
  if ((retval = ML605StartEthernet(fd605, SFP_RX_START)) < 0) {
    printf("Set loopback bit failed. Return %d\n", retval);
    return;
  }

	j = 0;

	while(1)
	{
//		printf("readbackBuf=0x%lx\n", readbackBuf);

    if ((bytes = ML605Recv(fd605, readbackBuf, kTestLen)) < kTestLen) {
      printf("Recv failed\n");
      return;
    }
    
    if (isFirstTime)
    {
      isFirstTime = false;
//      unsigned short *ptr_usample16 = (unsigned short*)readbackBuf;
      short *ptr_sample16 = (short*)readbackBuf;
      for (int tmp_index = 0; tmp_index < 32; ++tmp_index)
      {
//        printf("%5d\t%5d\n", (ptr_usample16[tmp_index*2]&0x0FFF)-2048, (ptr_usample16[tmp_index*2+1]&0x0FFF)-2048);
        printf("%5d\t%5d\n", ptr_sample16[tmp_index*2], ptr_sample16[tmp_index*2+1]);
      }
    }
/*
    counter_ms = ML605GetHwCounterMs(fd605);
    delay_tx2rx = counter_ms - *ptr32_buffer;
    if (delay_tx2rx < 0) {
			delay_tx2rx += 1000;
    }
    printf("%d\n", delay_tx2rx);

		if (delay_tx2rx > 9) {
			++arr_delay[9];
		} else {
			++arr_delay[delay_tx2rx];
		}
		++frame_counter;

		// test for 1,000 seconds (1.0e6 sub-frames)
		if (frame_counter >= 1000*1000) {
			break;
		}
*/

#ifdef RX_VERIFY
//    if (write(dst_fd, readbackBuf, kTestLen) < kTestLen) {
//      printf("Write destination file %s error\n", DST_FILENAME);
//    }

		num_error = 0;
//		printf("Read back values:\n");
		for (i = 0; i < kTestLen/4; ++i) {
//		  printf("%d\t", readbackBuf[i]);
//		  if (15 == i%16) {
//		    printf("\n");
//		  }
		  if (readbackBuf[i] != testBuf[i]) {
		    ++num_error;
				printf("Index=%d, src=%x, dst=%x\n", i, testBuf[i], readbackBuf[i]);
		  }
			readbackBuf[i] = 0;
		}

		if (num_error) {
		  printf("Error bytes = %d, loop %d\n", num_error, j);
		} else {
//		  printf("========== Correct loop %d ===========\n", j);
		}

		++j;
#endif
  }

	// print test results
	printf("Total Rx sub-frames = %d\nTx-to-Rx delay statistics (times, percentage):\n", frame_counter);
	for (i = 0; i < 9; ++i) {
		printf("%d ms: %d, %e\n", i+1, arr_delay[i], (double)(arr_delay[i])/(double)(frame_counter));
	}
	printf("> 9 ms: %d, %e\n", arr_delay[9], (double)(arr_delay[9])/(double)(frame_counter));
}

void FileTest() {
  int i, j;
	struct timeval tv_start, tv_finish;
	double dou_time;
	int16_t *ptr16_readback_buf;
  int retval;
#ifndef READ_ONLY
	int src_fd;
#endif	
	int dst_fd;

#ifndef READ_ONLY
  if ((src_fd = open(SRC_FILENAME, O_RDONLY)) < 0) {
    printf("Failed open %s\n", SRC_FILENAME);
    return;
  }
#endif

  if ((dst_fd = open(DST_FILENAME, O_WRONLY|O_CREAT|O_TRUNC,
       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    printf("Failed open %s\n", DST_FILENAME);
    return;
  }

  unsigned char testBuf[kTestLen*kAntNum];
  unsigned char readbackBuf[kTestLen*kAntNum];

	ptr16_readback_buf = reinterpret_cast<int16_t*>(&(readbackBuf[0]));

  for (i = 0; i < kTestLen*kAntNum; ++i) {
    testBuf[i] = i;
		readbackBuf[i] = 0;
  }

  int bytes = 0;

#ifndef READ_ONLY
	// write test
  // the first 1 ms can't be transmitted correctly by FPGA
  // so we send garbage here
  if ((bytes = ML605Send(fd605, testBuf, kTestLen*kAntNum)) < kTestLen*kAntNum) {
    printf("Send failed\n");
    return;
  }
	gettimeofday(&tv_start, NULL);
	for (j = 0; j < kTestTimes; ++j)
  {
//		printf("%d\n", j);
    if (read(src_fd, testBuf, kTestLen*kAntNum) < kTestLen*kAntNum) {
      printf("Read source file %s error\n", SRC_FILENAME);
    }

//		for (i = 0; i < 8; ++i) {
//			printf("%2x\t", testBuf[i]);
//		}
//		printf("\n");

    //usleep(5000);
    if ((bytes = ML605Send(fd605, testBuf, kTestLen*kAntNum)) < kTestLen*kAntNum) {
      printf("Send failed\n");
      return;
    }
	}
	gettimeofday(&tv_finish, NULL);
	dou_time =  tv_finish.tv_sec
						+ tv_finish.tv_usec/1000000.0
						- tv_start.tv_sec
						- tv_start.tv_usec/1000000.0;
  printf("Write speed: %f Gbps (with src file read)\n", kTestLen*kTestTimes*8/dou_time/1.0e9);
//	return 0;

  if ((retval = ML605StartEthernet(fd605, SFP_TX_START)) < 0) {
    printf("Set loopback bit failed. Return %d\n", retval);
    close(src_fd);
    close(dst_fd);
    return;
  }

  if ((retval = ML605StartEthernet(fd605, SFP_RX_START)) < 0) {
    printf("Set loopback bit failed. Return %d\n", retval);
    close(src_fd);
    close(dst_fd);
    return;
  }

while(0) {  
	for (j = 0; j < kTestTimes; ++j)
  {
//		printf("%d\n", j);
    if (read(src_fd, testBuf, kTestLen*kAntNum) < kTestLen*kAntNum) {
      lseek(src_fd, 0, SEEK_SET);
      read(src_fd, testBuf, kTestLen*kAntNum);
//      printf("Read source file %s error\n", SRC_FILENAME);
    }

//		for (i = 0; i < 8; ++i) {
//			printf("%2x\t", testBuf[i]);
//		}
//		printf("\n");

    //usleep(5000);
    if ((bytes = ML605Send(fd605, testBuf, kTestLen*kAntNum)) < kTestLen*kAntNum) {
      printf("Send failed\n");
      return;
    }
	}
}  
//	  usleep(1000);

#ifdef WRITE_ONLY
	close(src_fd);
	close(dst_fd);
	return;
#endif	// WRITE_ONLY
#endif	// READ_ONLY

#ifdef READ_ONLY
  if ((retval = ML605StartEthernet(fd605, SFP_RX_START)) < 0) {
    printf("Set loopback bit failed. Return %d\n", retval);
    return;
  }
#endif

  // read test
#if 1  
  // Discard 3 subframes [2ms-byte-delay, 1ms-byte-garbage]
  if ((bytes = ML605Recv(fd605, readbackBuf, kTestLen*kAntNum)) < kTestLen*kAntNum) {

    printf("Recv failed\n");
    return;
  }
  if ((bytes = ML605Recv(fd605, readbackBuf, kTestLen*kAntNum)) < kTestLen*kAntNum) {

    printf("Recv failed\n");
    return;
  }
  if ((bytes = ML605Recv(fd605, readbackBuf, kTestLen*kAntNum)) < kTestLen*kAntNum) {
    printf("Recv failed\n");
    return;
  }
#endif

	gettimeofday(&tv_start, NULL);
	// Need receive 1 more subframe, because delay = 2 SF + 1 sample.
	for (j = 0; j < kTestTimes + 1; ++j)
	{
	  static const int kSampleDelay = 1;
    if ((bytes = ML605Recv(fd605, readbackBuf, kTestLen*kAntNum)) < kTestLen*kAntNum) {
      printf("Recv failed\n");
      return;
    }

		for (i = 0; i < kTestLen*kAntNum/2; ++i) {
			ptr16_readback_buf[i] = ptr16_readback_buf[i] << 2;   // for frame sync position test purpose, we don't need shift it.
		}
		
//		for (int rx_idx = 0; rx_idx < 16; ++rx_idx) {
//		  printf("%5d %5d\n", ptr16_readback_buf[rx_idx*2], ptr16_readback_buf[rx_idx*2+1]);
//		}

    if (write(dst_fd, readbackBuf, kTestLen*kAntNum) < kTestLen*kAntNum) {
      printf("Write destination file %s error\n", DST_FILENAME);
    }
  }
	gettimeofday(&tv_finish, NULL);
	dou_time =  tv_finish.tv_sec
						+ tv_finish.tv_usec/1000000.0
						- tv_start.tv_sec
						- tv_start.tv_usec/1000000.0;
  printf("Read speed: %f Gbps (with dst file write)\n", kTestLen*kTestTimes*8/dou_time/1.0e9);

	printf("Test size = %d\n", kTestLen);
	printf("Test loop = %d\n", kTestTimes);
	printf("=============================================\n");
	printf("Please run MD5 check\n");

#ifndef READ_ONLY
	close(src_fd);
#endif
	close(dst_fd);
}

void FileGen() {
  int src_fd;
  int i, j;
  int randBuf[kTestLen*kAntNum];
	int num_sign_bits;

  int num_time = time(NULL);
  srand(num_time);

  printf("Generating random file\n");

  if ((src_fd = open(SRC_FILENAME, O_WRONLY|O_CREAT|O_TRUNC,
       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    printf("Failed open %s\n", SRC_FILENAME);
    return;
  }

  int cnt = 0;
  
  for (i = 0; i < kTestTimes; ++i) {
    ++cnt;
    for (j = 0; j < kTestLen/(int)sizeof(int)*kAntNum; ++j) {
#if 0
      if ((j & 0x01) == 0) {
        randBuf[j] = ((cnt*2) << 18) | ((cnt*2+1) << 2);
      } else {
        randBuf[j] = (((cnt*2) << 18) | ((cnt*2+1) << 2)) ^ 0xFFFCFFFC;
      }
      
      if ((j == 0) || (j == 1)) {
        randBuf[j] = ((i*2+j) << 24) | ((i*2+j) << 20) | ((i*2+j) << 8) | ((i*2+j) << 4);
      }
      
      if (j >= kTestLen/(int)sizeof(int)*kAntNum/2) {
        randBuf[j] = randBuf[j] << 2;
      }
#else    
      randBuf[j] = rand() & 0x9FFC9FFC;   // FPGA will only transfer 12 in every 16 bits
//			printf("===========================\nBuf=%x\n", randBuf[j]);
			num_sign_bits = randBuf[j] & 0x80008000;
//			printf("sign=%x\n", num_sign_bits);
			randBuf[j] |= num_sign_bits >> 1;
//			printf("Buf=%x\n", randBuf[j]);
			randBuf[j] |= num_sign_bits >> 2;
//			printf("Buf=%x\n", randBuf[j]);
//      randBuf[j] = rand() & 0xFFF0FFF0;   // FPGA will only transfer 12 in every 16 bits
//			randBuf[j] = (j<<4) & 0xFFF0FFF0;   // FPGA will only transfer 12 in every 16 bits
#endif
    }

    if (write(src_fd, randBuf, kTestLen*kAntNum) < kTestLen*kAntNum) {
      printf("Write %s error\n", SRC_FILENAME);
      return;
    }
//    printf("File gen: i=%d, j=%d\n", i, j);
  }

  printf("Completed\n");

	close(src_fd);
}

void RdCounterTest() {
  int counter_ms;
  if ((counter_ms = ML605GetHwCounterMs(fd605)) < 0) {
    printf("Read counter failed\n");
  } else {
    printf("Counter = %d ms\n", counter_ms);
  }

  return;
}

void SetRfCmd(int cmd) {
	ML605SetRfCmd(fd605, cmd);
}

static const int kPeriod = 16;  // frequency = 30.72/16 = 1.92 MHz
static const double kAmplitude = 2000.0;
static const double kAmplitude_B = 1000.0;

void SinTx() {
  static const int kFrameLen = 30720;
  static const int kAntNum = 2;
  Complex16 test_sin_wave_a[kFrameLen][kAntNum];
  Complex16 test_sin_wave_b[kFrameLen][kAntNum];
  for (int i = 0; i < kFrameLen/kPeriod; ++i) {
    for (int j = 0; j < kPeriod; ++j) {
      // Antenna 0
      test_sin_wave_a[i*kPeriod + j][0].n16_real = kAmplitude *
        cos(2.0*M_PI*static_cast<double>(j)/static_cast<double>(kPeriod)) * 4;
      test_sin_wave_a[i*kPeriod + j][0].n16_imag = kAmplitude *
        sin(2.0*M_PI*static_cast<double>(j)/static_cast<double>(kPeriod)) * 4;

      test_sin_wave_b[i*kPeriod + j][0].n16_real = kAmplitude_B *
        cos(2.0*M_PI*static_cast<double>(j)/static_cast<double>(kPeriod)) * 4;
      test_sin_wave_b[i*kPeriod + j][0].n16_imag = kAmplitude_B *
        sin(2.0*M_PI*static_cast<double>(j)/static_cast<double>(kPeriod)) * 4;      
    }
  }

#if 0
	// time offset - imag+1
	int16_t n16_tmp = test_sin_wave[kFrameLen-1].n16_imag;
	for (int i = kFrameLen-1; i > 0; --i) {
		test_sin_wave[i].n16_imag = test_sin_wave[i-1].n16_imag;
	}
	test_sin_wave[0].n16_imag = n16_tmp;
#endif
#if 0
	// time offset - imag-1
	int16_t n16_tmp = test_sin_wave[0].n16_imag;
	for (int i = 0; i < kFrameLen-1; ++i) {
		test_sin_wave[i].n16_imag = test_sin_wave[i+1].n16_imag;
	}
	test_sin_wave[kFrameLen-1].n16_imag = n16_tmp;
#endif

  // test
  for (int i = 0; i < 2*kPeriod; ++i) {
    printf("[%d]Real=%d, Imag=%d\n", i, test_sin_wave_a[i][0].n16_real, test_sin_wave_a[i][0].n16_imag);
  }

  int bytes = 0;
	int i;
	int tx_frame_num = 0;
	void *ptr_tx_wave = (void *)test_sin_wave_a;

	for (i = 0; i < kTestTimes; ++i) {
    if (((tx_frame_num++) & 0x1) == 0) {
      ptr_tx_wave = (void *)test_sin_wave_a;
    } else {
      ptr_tx_wave = (void *)test_sin_wave_b;
    }
    if ((bytes = ML605Send(fd605, ptr_tx_wave, kTestLen*kAntNum)) < kTestLen*kAntNum) {
      printf("Send failed. loop %d\n", i);
      return;
    }
    usleep(100);
	}
	
	printf("okay\n");
	
  int retval;
  if ((retval = ML605StartEthernet(fd605, SFP_TX_START)) < 0) {
    printf("Set loopback bit failed. Return %d\n", retval);
    return;
  }
	
	// write test
	i = 0;
	while(1)
  {
    if (((tx_frame_num++) & 0x1) == 0) {
      ptr_tx_wave = (void *)test_sin_wave_a;
    } else {
      ptr_tx_wave = (void *)test_sin_wave_b;
    }  
    if ((bytes = ML605Send(fd605, ptr_tx_wave, kTestLen*kAntNum)) < kTestLen*kAntNum) {
      printf("Send failed\n");
      return;
    }
    usleep(100);
//		printf("%d\n", i++);
	}
}

void IncrementWrite() {
  short arr16_src_buff[30720*2];
  short n16_value = 0;

  for (int i = 0; i < 30720*2; ++i) {
    arr16_src_buff[i] = n16_value;
    n16_value = (n16_value == 2047) ? 0 : (n16_value+1);
  }

	// write kDelay+2 ms data before start Tx
	int bytes = 0;
	for (int i = 0; i < kTestTimes; ++i) {
    if ((bytes = ML605Send(fd605, arr16_src_buff, kTestLen)) < kTestLen) {
      printf("Send failed. loop %d\n", i);
      return;
    }
	}

	printf("Pre-buffering completed\n");

	while (ML605GetHwCounterMs(fd605) != 950) {
		;
	}

  int retval = 0;
	if ((retval = ML605StartEthernet(fd605, SFP_TX_START)) < 0) {
		printf("Set loopback bit failed. Return %d\n", retval);
		return;
	}

	while (ML605GetHwCounterMs(fd605) >= 900) {
		;
	}

	// write test
	int i = 0;
	int counter_ms, counter_50mhz, temp_ms;
	int diff_ms = 0;
	int prev_ms = 0;
//	gettimeofday(&tv_start, NULL);
	while(1) {
		ML605GetHwCounters(fd605, &counter_ms, &counter_50mhz);
		// in order to avoid counter glitch
		while (i < 5) {
			ML605GetHwCounters(fd605, &temp_ms, &counter_50mhz);
			if (temp_ms == counter_ms) {
				++i;
			} else {
				i = 0;
				counter_ms = temp_ms;
			}
		}

		diff_ms = counter_ms - prev_ms;

		if (diff_ms < 0) {
			diff_ms += 1000;
		}

		prev_ms = counter_ms;

    // If we want 1 ms delay, then we can only write 1 sub-frame each time, even if the interval is longer than 1 ms.
    // Otherwise, we occupies more buffer and thus higher delay.
    if (diff_ms > kDelay) {
      printf("Error: Delay too high %d > %d ms\n", diff_ms, kDelay);
    } 
    int tx_frames = diff_ms;

		for (i = 0; i < tx_frames; ++i) {
		  if ((bytes = ML605Send(fd605, testBuf, kTestLen)) < kTestLen) {
		    printf("Send failed\n");
		    return;
		  }
		}
	}
	
  return;
}

void OfdmTx() {
  int src_fd;

  if ((src_fd = open(OFDM_SRC_FILENAME, O_RDONLY)) < 0) {
    printf("Failed open %s\n", OFDM_SRC_FILENAME);
    return;
  }
  if (read(src_fd, testBuf, kTestLen) < kTestLen) {
    printf("Read source file %s error\n", OFDM_SRC_FILENAME);
    return;
  }

	close(src_fd);

  int bytes = 0;
	int i;

	for (i = 0; i < kTestTimes; ++i)
  {
    if ((bytes = ML605Send(fd605, testBuf, kTestLen)) < kTestLen) {
      printf("Send failed. loop %d\n", i);
      return;
    }
	}

	printf("okay\n");

  int retval;
  if ((retval = ML605StartEthernet(fd605, SFP_TX_START)) < 0) {
    printf("Set loopback bit failed. Return %d\n", retval);
    return;
  }

	// write test
	i = 0;
	while(1)
  {
    if ((bytes = ML605Send(fd605, testBuf, kTestLen)) < kTestLen) {
      printf("Send failed\n");
      return;
    }
//		printf("%d\n", i++);
	}

  return;
}

int main() {
  int retval;

  setpriority(PRIO_PROCESS, 0, -18);

  if ((fd605 = ML605Open()) < 0) {
    printf("Device open failed. Return %d\n", fd605);
    return fd605;
  }

  for (int i = 0; i < kTestLen/4; ++i) {
//    testBuf[i] = (i<<20) | ((i<<4) & 0x0000FFFF);
//		testBuf[i] = i & 0xFFF0FFF0;
		testBuf[i] = 0;
		readbackBuf[i] = 0;
  }

	SetRfCmd(0x79007900);
	sleep(1);
#ifdef SEU_RF	
	SetRfCmd(0x58505950);
#endif	
#ifdef EAGLE_RF	
	SetRfCmd(0x58505950);
#endif
	sleep(1);
#ifdef SEU_RF
	SetRfCmd(0x48544954);
#endif	
#ifdef EAGLE_RF	
	SetRfCmd(0x48204920);
#endif
//  sleep(1);
  //SetRfCmd(0x00005818);
  //sleep(1);
	//SetRfCmd(0x59005900);
  RdCounterTest();
//  FileGen();
  RdCounterTest();
//  FileTest();
  RdCounterTest();
//  SpeedTest();

#ifdef WRITE_ONLY
//	InfiniteWrite();
  SinTx();
//	OfdmTx();
#endif

#ifdef READ_ONLY
//	InfiniteRead();
	FileTest();
#endif

#ifdef SFP_TEST
  IncrementWrite();
#endif

  if ((retval = ML605Close(fd605)) < 0) {
    printf("Device close failed. Return %d\n", retval);
    return retval;
  }

  return 0;
}
