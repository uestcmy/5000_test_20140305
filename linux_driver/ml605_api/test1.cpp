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
#include <pthread.h>

#include "ml605_api.h"
#include "ml605_api.cpp"
#include "xpmon_be.h"
#define RAWDATA_FILENAME    "/dev/ml605_raw_data"
#define XDMA_FILENAME       "/dev/xdma_stat"
#define PKTSIZE             4096
int testfd;
int main()
{

if((testfd = ML605Open())<0)
   printf("open ml605 failed");

/*if(ML605StartEthernet(testfd, SFP_TX_START)<0) {
    printf("PCIe:Start ethernet failure\n");
	  ML605Close(testfd);
    exit(-1);
  }
*/
 unsigned char txbuff[PKTSIZE];
for(int i=0;i++;i<PKTSIZE)
    txbuff[i] = i%2;
  int sendsize = ML605Send(testfd,txbuff,PKTSIZE);
	   if(sendsize==PKTSIZE)printf("send successful!\n");
usleep(1000000);

/*if(ML605StartEthernet(testfd, SFP_TX_START)<0) {
    printf("PCIe:Start ethernet failure\n");
	  ML605Close(testfd);
    exit(-1);
  }
*/
  unsigned char rxbuff[PKTSIZE];
int recvsize = ML605Recv(testfd,rxbuff,PKTSIZE);
	   if(recvsize==PKTSIZE)printf("error=%d\n",recvsize);
  char ch_input;
  scanf("%c", &ch_input);
  ML605Close(testfd);
}






