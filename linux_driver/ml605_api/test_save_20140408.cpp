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
#define SENDSIZE 6144 
#define BYTESIZE 12288
#define DATASIZE 6144
#include "socket_20140311.cpp"
#include "convert_20140311.cpp"
#include "judge_20140311.cpp"


int status = 0;
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
char send_buff[SENDSIZE*4];
int cnt_ofdm = 0;
int cnt_frame = 0;
int nextpos = 0;


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
void print_uchar(){
//    fprintf(fp2,"The whole ofdm frame is :");
     char buff2[10];
     memset(send_buff,0,sizeof(char)*SENDSIZE*4);
    
    for( int i = 0 ; i < SENDSIZE ; i++ ){
//        fprintf(fp2,"%x,",ofdm_buff[i]);
        convert_hex2str(ofdm_buff[i],buff2);
	 strcat(send_buff,buff2); 
        //  printf("%x,",ofdm_buff[i]);
    }
/*
     printf("ofdmchar is \n");
    for( int i = 0 ; i < 4804 ; i++ ){
       printf("%x,",ofdm_buff[i]);
    }printf("\n");
*/
    //socket_send(send_buff);
    //fprintf(fp2,"\n");
    printf("\n%d",cnt_frame);
    for( int i = 0 ; i < 30 ; i++){
        printf("%c",send_buff[i]);
    } 
    cnt_frame++;
}
int find_header(unsigned char *p){
   for( int i = 0 ; i < size ; i++ ){
      if(p[i] == 0xaa && p[i+1] == 0xa0  && p[i+2] == 0xaa  &&  p[i+3] == 0xa0 ){
            return i;
      }
   } 
  return -1;
}
void* myread(void* param)
{
        int lastpos = 0;
        int  begin = -1;
	unsigned char rxbuff[size];
	while(!isclose){    //usleep(1);
        	int recvsize = ML605Recv(testfd,rxbuff,4096);
	   	if(recvsize!=size)
			printf("recv error!\n");
        	else{
                       // printf("\nrecv successful!:");
/*
                        for( int i = 0 ; i < 10 ; i ++ ){
                             printf("%x,",rxbuff[i]);
                        }
*/
     			mycount = mycount + 1;
/*
                       if( mycount < 100 ){
                            for( int i = 0 ; i < size ; i++){
                                fprintf(fp1,"%x,",rxbuff[i]); 
                            }//end for 
                       }// end if
*/
                       if(status == 0 ){
                            int tmp =  find_header(rxbuff);
                            if( tmp != -1 ){//find the header 
                               /* 
				int span  = tmp-lastpos;//interval of crr and last frame per bytes
                                if( span < 0 ){
                                    span += 4096;
                                }
                                //printf("\n %d begin is :%d",mycount,span);
                                
                                if(span != 0 ){//if error occur
                                    printf("%d error\n",mycount);
                                    fprintf(fp1,"%d",mycount%10);
                                    if(mycount %10 == 0 ){
                                        fprintf(fp1,"\n");
                                    }
                                }
                                lastpos = tmp;
                              */
                              status = 1;//do not search the header any more
                              //printf("status : %d\n",status);
                                  cnt_ofdm = 0;
                              for( int i = tmp ; i < size ; i++){
                                  ofdm_buff[cnt_ofdm++] = rxbuff[i]; 
                              } 
                              
                            }                           
                       }//if status == 0 
                       else if( status == 1 ){
                            for( int i = 0 ; i < size && cnt_ofdm < BYTESIZE ; i++ ){
                                ofdm_buff[cnt_ofdm++] = rxbuff[i];
                                if( cnt_ofdm == BYTESIZE ) {
                                    nextpos = i;
                                    printf("next pos : %d\n",nextpos);
                                    break;
                                }  
                            }
                            if(cnt_ofdm == BYTESIZE){//recv completed
                                cnt_ofdm = 0;//reset
                                //send through socket...
                                print_uchar();
                                if(rxbuff[nextpos+1] != 0xaa ){
                                     status = 0;
                                     cnt_ofdm = 0;
                                     }
                                for( int i = nextpos+1 ; i < size ; i++ ){
                                    ofdm_buff[cnt_ofdm++] = rxbuff[i];
                                     }                              
                                 }
                       }// end elif
/*
                       if( begin != -1 ){
                           status = 0;

                       }

                       if(begin >= 0 && begin < 4096){
                           printf("pos : %x\n",rxbuff[begin+10]);              
                           begin = begin + 10 + 4096*2;
                       }else if(begin != -1){
                           begin -= 4096;
                       }   
                       
  */                                                   
                      

        	}//end else
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
