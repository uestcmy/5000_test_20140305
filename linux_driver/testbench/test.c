#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

#include "xpmon_be.h"

// #define RX_VERIFY
// #define NO_WRITE

#define RAWDATA_FILENAME    "/dev/ml605_raw_data"
#define XDMA_FILENAME       "/dev/xdma_stat"
#define PKTSIZE     4096

#define SRC_FILENAME        "./src.dat"
#define DST_FILENAME        "./dst.dat"

/* File pointer for the raw data file */
static int rawdatafd = -1;

/* File pointer for the xdma status file */
static int xdmadatafd = -1;

//static const int kTestTimes = 32768;	// 32768 * 4096 = 128 MB
static const int kTestTimes = 10000;
static const int kTestLen = 4096;

void SpeedTest() {
  int i, j;
  int num_slow_tx, num_slow_rx;
  int retval;
	struct timeval tv_start, tv_finish;
	double dou_time;

#ifdef RX_VERIFY
  int num_error;
#endif

  TestCmd testCmd;
  testCmd.Engine = 1;
  testCmd.TestMode = TEST_START | ENABLE_LOOPBACK;
  testCmd.MinPktSize = testCmd.MaxPktSize = PKTSIZE;

  unsigned char testBuf[kTestLen];
  unsigned char readbackBuf[kTestLen];

  for (i = 0; i < kTestLen; ++i) {
    testBuf[i] = i;
  }

  int bytes = 0;
	printf("ISTART_TEST=0x%lx\n", ISTART_TEST);
  retval = ioctl(xdmadatafd, ISTART_TEST, &testCmd);
  if(retval != 0)
  {
    printf("START_TEST on Eng %d failed\n", testCmd.Engine);
  }


#ifndef NO_WRITE
	// write test
	num_slow_tx = 0;
	gettimeofday(&tv_start, NULL);
	for (j = 0; j < kTestTimes; ++j)
  {
//		printf("%d\n", j);
		while ((bytes = write(rawdatafd, testBuf, kTestLen)) <= 0) {
//		  perror("Write to ML605 failed\n");
		  usleep(1000);
			++num_slow_tx;
			if (num_slow_tx > 10000) {
				printf("Tx wait over 10,000 times. Exit.\n");
				return;
			}
		}
	}
	gettimeofday(&tv_finish, NULL);
	dou_time =  tv_finish.tv_sec
						+ tv_finish.tv_usec/1000000.0
						- tv_start.tv_sec
						- tv_start.tv_usec/1000000.0;
	printf("Write completed. Elapsed time: %f seconds.\n", dou_time);
	printf("Write speed = %e bps\n", kTestTimes*kTestLen*8.0/dou_time);

//	return 0;
#endif

//	  usleep(1000);

	// read test
	num_slow_rx = 0;
	gettimeofday(&tv_start, NULL);
	for (j = 0; j < kTestTimes; ++j)
	{
		while ((bytes = read(rawdatafd, readbackBuf, kTestLen)) <= 0) {
//			printf("Loop %d: read failed, return = %d, buf=0x%lx\n", j, bytes, (long unsigned)readbackBuf);
			++num_slow_rx;
//			return -1;
		  usleep(1000);
			if (num_slow_rx > 10000) {
				printf("Rx wait over 10,000 times. Exit.\n");
				return;
			}
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
	printf("Read speed = %e bps\n", kTestTimes*kTestLen*8.0/dou_time);

	printf("=============================================\n");
  printf("Slow tx = %d\n", num_slow_tx);
	printf("Slow rx = %d\n", num_slow_rx);
	printf("Test size = %d\n", kTestLen);
	printf("Test loop = %d\n", kTestTimes);
}


void FileTest() {
  int i, j;
  int num_slow_tx, num_slow_rx;
  int retval;
	struct timeval tv_start, tv_finish;
	double dou_time;

	int src_fd, dst_fd;

  TestCmd testCmd;
  testCmd.Engine = 1;
  testCmd.TestMode = TEST_START | ENABLE_LOOPBACK;
  testCmd.MinPktSize = testCmd.MaxPktSize = PKTSIZE;

  if ((src_fd = open(SRC_FILENAME, O_RDONLY)) < 0) {
    printf("Failed open %s\n", SRC_FILENAME);
    return;
  }

  if ((dst_fd = open(DST_FILENAME, O_WRONLY|O_CREAT|O_TRUNC,
       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    printf("Failed open %s\n", DST_FILENAME);
    return;
  }

  unsigned char testBuf[kTestLen];
  unsigned char readbackBuf[kTestLen];

  for (i = 0; i < kTestLen; ++i) {
    testBuf[i] = i;
  }

  int bytes = 0;
	printf("ISTART_TEST=0x%lx\n", ISTART_TEST);
  retval = ioctl(xdmadatafd, ISTART_TEST, &testCmd);
  if(retval != 0)
  {
    printf("START_TEST on Eng %d failed\n", testCmd.Engine);
  }


#if 1
	// write test
	num_slow_tx = 0;
	gettimeofday(&tv_start, NULL);
	for (j = 0; j < kTestTimes; ++j)
  {
//		printf("%d\n", j);
    if (read(src_fd, testBuf, kTestLen) < kTestLen) {
      printf("Read source file %s error\n", SRC_FILENAME);
    }

		while ((bytes = write(rawdatafd, testBuf, kTestLen)) <= 0) {
//		  perror("Write to ML605 failed\n");
		  usleep(1000);
			++num_slow_tx;
		}
	}
	gettimeofday(&tv_finish, NULL);
	dou_time =  tv_finish.tv_sec
						+ tv_finish.tv_usec/1000000.0
						- tv_start.tv_sec
						- tv_start.tv_usec/1000000.0;

//	return 0;
#endif

//	  usleep(1000);

	// read test
	num_slow_rx = 0;
	gettimeofday(&tv_start, NULL);
	for (j = 0; j < kTestTimes; ++j)
	{
		while ((bytes = read(rawdatafd, readbackBuf, kTestLen)) <= 0) {
//			printf("Loop %d: read failed, return = %d, buf=0x%lx\n", j, bytes, (long unsigned)readbackBuf);
			++num_slow_rx;
//			return -1;
		  usleep(1000);
		}

    if (write(dst_fd, readbackBuf, kTestLen) < kTestLen) {
      printf("Write destination file %s error\n", DST_FILENAME);
    }
  }
	gettimeofday(&tv_finish, NULL);
	dou_time =  tv_finish.tv_sec
						+ tv_finish.tv_usec/1000000.0
						- tv_start.tv_sec
						- tv_start.tv_usec/1000000.0;
	printf("Test size = %d\n", kTestLen);
	printf("Test loop = %d\n", kTestTimes);
	printf("=============================================\n");
	printf("Please run MD5 check\n");


	close(src_fd);
	close(dst_fd);
}

void FileGen() {
  int src_fd;
  int i, j;
  int randBuf[kTestLen];

  int num_time = time(NULL);
  srand(num_time);

  if ((src_fd = open(SRC_FILENAME, O_WRONLY|O_CREAT|O_TRUNC,
       S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)) < 0) {
    printf("Failed open %s\n", SRC_FILENAME);
    return;
  }

  for (i = 0; i < kTestTimes/4; ++i) {
    for (j = 0; j < kTestLen; ++j) {
      randBuf[j] = rand();
    }

    if ((write(src_fd, randBuf, kTestLen*sizeof(int))) < (kTestLen*sizeof(int))) {
      printf("Write %s error\n", SRC_FILENAME);
      return;
    }
  }

	close(src_fd);
}


int main() {
  if ((rawdatafd = open(RAWDATA_FILENAME, O_RDWR)) < 0) {
    printf("Failed open %s\n", RAWDATA_FILENAME);
		return -1;
  }
  if ((xdmadatafd = open(XDMA_FILENAME, O_RDONLY)) < 0) {
    printf("Failed open %s\n", XDMA_FILENAME);
    return -1;
  }

	SpeedTest();

//	FileGen();

//  FileTest();

	close(rawdatafd);
	close(xdmadatafd);

  return 0;
}
