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




#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <arpa/inet.h>

#define SERVPORT 6001
#define SERVPORT2 6002


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

    int sock_fd, recvbytes;
    int sock_fd2, recvbytes2;
    int sock_fd3, recvbytes3;
    struct hostent *host;
    struct sockaddr_in serv_addr;
    struct sockaddr_in serv_addr2;
    struct sockaddr_in serv_addr3;



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
            
         unsigned char tmp = ofdm_buff[i];
				int bit[8] = {0};
				int cntbit = 0;
				char buff[3];
				char buff2[4]= {0};
				while(tmp>0){
					bit[cntbit++] =tmp % 2;
					//printf("%d",bit);
					tmp /= 2;				
				}
				int base = 8;
				int sum = 0;
				for( int j = 7 ; j >= 3 ; j-- ){
					sum += bit[j] * base;
					base /= 2;		
				}
				if( sum < 10 ){
					sprintf(buff,"%c",sum+'0');
				}else{
					sprintf(buff,"%c",sum-10+'a');
				}
				strcat(buff2,buff);
				base = 8;
				sum = 0;
				for( int j = 3 ; j >= 0 ; j-- ){
					sum += bit[j] * base;
					base /= 2;		
				}
				if( sum < 10 ){
					sprintf(buff,"%c",sum+'0');
				}else{
					sprintf(buff,"%c",sum-10+'a');
				}
				strcat(buff2,buff);
				
				buff2[2] = ',';
				
				strcat(send_buff,buff2); 

    }
    
    sendto(sock_fd,send_buff,14404,0,(struct sockaddr *)&serv_addr,sizeof(serv_addr));//port:7005 , send to QPSK plot GUI 
    sendto(sock_fd2,send_buff,14404,0,(struct sockaddr *)&serv_addr2,sizeof(serv_addr));//port:7004, send to channel plot GUI
    sendto(sock_fd3,send_buff,14404,0,(struct sockaddr *)&serv_addr3,sizeof(serv_addr));//port:7003, send to channel plot GUI
     
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
                            (rxbuff[0] == 0xaa &&   rxbuff[1] == 0xa0 &&
                             rxbuff[2] == 0xaa &&   rxbuff[3] == 0xa0 
                            )||(
                             rxbuff[0] == 0xa0 && rxbuff[1] == 0xaa &&
                             rxbuff[2] == 0xa0 && rxbuff[3] == 0xaa 
                            )
                              

                           ){
                            if(
                                 ( rxbuff[size-2] != 0xaa && rxbuff[size-1] != 0xa0 ) && ( rxbuff[size-2] != 0xa0 && rxbuff[size-1] != 0xaa) 
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

    host=gethostbyname("127.0.0.1");
    sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
    serv_addr.sin_family=AF_INET;
    serv_addr.sin_port=htons(7005);
    serv_addr.sin_addr = *((struct in_addr *)host->h_addr);
    bzero(&(serv_addr.sin_zero),8);

    sock_fd2 = socket(AF_INET, SOCK_DGRAM, 0);
    serv_addr2.sin_family=AF_INET;
    serv_addr2.sin_port=htons(7004);
    serv_addr2.sin_addr = *((struct in_addr *)host->h_addr);
    bzero(&(serv_addr2.sin_zero),8);

    sock_fd3 = socket(AF_INET, SOCK_DGRAM, 0);
    serv_addr3.sin_family=AF_INET;
    serv_addr3.sin_port=htons(7003);// for Channel H11
    serv_addr3.sin_addr = *((struct in_addr *)host->h_addr);
    bzero(&(serv_addr3.sin_zero),8);




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
