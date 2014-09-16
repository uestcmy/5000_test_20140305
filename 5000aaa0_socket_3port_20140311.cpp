#include <stdio.h>
#include <string.h>
//#include <sys/types.h>
//#include <sys/stat.h>
//#include <fcntl.h>
//#include <unistd.h>
//#include <sys/ioctl.h>
#include <sys/time.h>
#include <time.h>
//#include <stdlib.h>
//#include <errno.h>
#include <pthread.h>
//#include "ml605_api.h"
#include "ml605_api.cpp"
//#include "xpmon_be.h"
//#define RAWDATA_FILENAME    "/dev/ml605_raw_data"
//#define XDMA_FILENAME       "/dev/xdma_stat"
//#define PKTSIZE             4096
#include "socket_20140311.cpp"
#include "convert_20140311.cpp"
#include "judge_20140311.cpp"


static const int size = 4096;
static const int recv_cycle = 100;
bool isclose = false;
int testfd;
int count=0;
long int mycount = 0;
int find_aaa0 = 0;
int mysleep = 1000000;


FILE *fp1,*fp2;
unsigned char sml_buff[size];
unsigned char ofdm_buff[size*4];
char send_buff[14404];
int cnt_ofdm = 0;
int status = 0;
int cnt_frame = 0;



void* mywrite(void* param)
{
unsigned char txbuff[size];

while(!isclose)
   {for(int i=0;i<size;i++)
    txbuff[i] = count;
       int sendsize = ML605Send(testfd,txbuff,size);
	   if(sendsize!=size)printf("send error!\n");
	   
	  // usleep(1);
    count++;
   }

}
void print_uchar(unsigned char * ofdm_b){
//    fprintf(fp2,"The whole ofdm frame is :");

     memset(send_buff,0,sizeof(char)*14404);
    for( int i = 0 ; i < 4804 ; i++ ){
        //fprintf(fp2,"%x,",ofdm_buff[i]);
		    convert_hex2str(ofdm_buff[i],buff2);
				strcat(send_buff,buff2); 
    }
    
    socket_send();
     
    //fprintf(fp2,"\n");
    printf("--------------------------------------------------------------------------------------------");
    cnt_frame++;
}

void* myread(void* param)
{
	unsigned char rxbuff[size];
	while(!isclose){    //usleep(1);
        	int recvsize = ML605Recv(testfd,rxbuff,4096);
	   	if(recvsize!=size)
			printf("recv error!\n");
        	else{
     			mycount = mycount + 1;
                        //printf("\n");
                        
                        if( mycount <= 100 ){
                            for( int j = 0 ; j < size ; j++ ){
                                fprintf(fp1,"%x,",rxbuff[j]);
                            }
                            fprintf(fp1,"\n");
			                   }
 
                        for(int j = 0 ; j < 10 ; j++){
                            printf("%x,",rxbuff[j]);
                        }

                        if(status == 1){
                            for( int i = 0 ; i <size && cnt_ofdm <= 4804; i++){
                               ofdm_buff[cnt_ofdm++] = rxbuff[i]; 
                            }

                            if( cnt_ofdm == 4805 ){
				                        status = 0;
                                if(1){
                                    print_uchar(ofdm_buff);
	                        }
                                
                                
                            } 
                        }

                        if( status == 0 &&
                            judge_header_a0aa(rxbuff)
                              

                           ){
                            if(
                                judge_tail_a0aa(rxbuff,size) 
                            ){
                                 int begin = 0;
                                 printf("20,3c is found!");
                                 for( int i = 0; i < size-4 ; i++ ){
                                     if( rxbuff[i] == 0xcc && rxbuff[i+1] == 0xcc
                                      ){
                                         printf("indeed!!!!");
                                         begin = i;
                                         break;
                                     }//if
                                 }//for
                                 if( begin != 0 ){
                                     status = 1;
                                     int cnt_sml = 0;
                                     cnt_ofdm = 0;
                                     memset(ofdm_buff,0,sizeof(char)*size*4);
                                     for(int i = begin ; i < size ; i++ ){
                                         ofdm_buff[cnt_ofdm++] = rxbuff[i];
                                     }
                                     printf("ofdm_buffer is :");
                                     for( int j = 0 ; j <10 ; j++ ){
                                          printf("%x,",ofdm_buff[j]);
                                     }   printf("\n"); 
                                 }
                            }//if

                            printf("\n");
                        }//if
                        else {
                              printf("Not find 00,aa\n");

                        }//else


/*
                        for(int i=0;i<4;i++){
//	          		printf("%x,",rxbuff[i]);
	                        if( rxbuff[i] == 0xaa && rxbuff[i+1] == 0xa0 ){
                                     printf("Find aa,a0");
                                     break;
                                }
                                else{
                                     printf("Not find aa,a0");
                                     break;
                                }
			}//for 

                        printf("\n");
*/
        	}//end else
		   
		//usleep(500000);

   	}// end while

}//myread
void* GetRate(void* param)
{   int scount;
    int ecount;
    double recvrata;
    while(!isclose){ 
          scount = mycount;
          printf("scount=%d\n",scount);
          usleep(1000000);
          ecount = mycount;
          printf("ecount=%d\n",ecount);
          recvrata = (ecount - scount)*4096*8/1000000;
          printf("receive data rata = %f M/S \n",recvrata);
}
}
int main()
{

   socket_init();

   fp1 = fopen("raw_recv_data.txt","w");
   fp2 = fopen("ofdm_frame.txt","w");

   printf("my:hello fedora!\n");
if((testfd = ML605Open())<0)
   printf("open ml605 failed");

if(ML605StartEthernet(testfd, SFP_TX_START)<0) {
    printf("PCIe:Start ethernet failure\n");
	  ML605Close(testfd);
    exit(-1);
  }

/* pthread_t writethread;
  if (pthread_create(&writethread, NULL, mywrite, NULL)) 
  {
    perror("writethread process thread creation failed");
  }  
  sleep(1);
*/  

/*
  pthread_t readthread;
  if (pthread_create(&readthread, NULL, GetRate, NULL)) 
  {
    perror("readthread process thread creation failed");
  }  
  sleep(1);
*/  
  pthread_t ratathread;
  if (pthread_create(&ratathread, NULL, myread, NULL)) 
  {
    perror("readthread process thread creation failed");
  }  
  sleep(1);


  char ch_input;
  scanf("%c", &ch_input);
  isclose=true;
  ML605Close(testfd);
  fclose(fp1);
  fclose(fp2);
}
