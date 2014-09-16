#include <stdio.h>
#include <sys/time.h>
#include <time.h>
#include <pthread.h>
#include "ml605_api.cpp"
static const int size = 4096;
bool isclose = false;
int testfd;
int count=0;
long int mycount = 0;
int mysleep = 1000000;

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

void* myread(void* param)
{
	const char filename[10][30] = {"./data/dataout1.txt","./data/dataout2.txt","./data/dataout3.txt","./data/dataout4.txt","./data/dataout5.txt",
						"./data/dataout6.txt","./data/dataout7.txt","./data/dataout8.txt","./data/dataout9.txt","./data/dataout10.txt"						
						};
	FILE *fp[20];
	int length = 10;
	for( int i = 0 ; i < length ; i++ ){
		fp[i] = fopen(filename[i],"w");
	}
	
	//freopen("dataout.txt","w",stdout);
	unsigned char rxbuff[size];
	while(!isclose){    //usleep(1);
        	int recvsize = ML605Recv(testfd,rxbuff,4096);
		int fileid = 0;
	   	if(recvsize!=size)
			printf("recv error!\n");
        	else{
			fileid = mycount / 20;
			fileid %= length;
			
			if( mycount % 20 == 0){			
				fp[fileid] = fopen(filename[fileid],"w");
				printf("fp %d is open.\n",fileid);
			}

     			mycount = mycount + 1;
	              printf("mycount is : %d\n",mycount);
			//printf("recv successful!=%d",recvsize);
		    
            		for(int i=0;i<size;i++){
	           		fprintf(fp[fileid],"%x",rxbuff[i]);
			}
            		//printf("one receive cycle\n");
			if( mycount % 20 == 0 ){
				fclose(fp[fileid]);
				printf("fp %d  is closed.\n",fileid);
			}
        	}//end else
		   
		//usleep(500000);

   	}// end while

	for( int i = 0 ; i < length ; i++){
		fclose(fp[i]);
	}

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


  pthread_t readthread;
  if (pthread_create(&readthread, NULL, GetRate, NULL)) 
  {
    perror("readthread process thread creation failed");
  }  
  sleep(1);
  
  pthread_t ratathread;
  if (pthread_create(&ratathread, NULL, myread, NULL)) 
  {
    perror("readthread process thread creation failed");
  }  
  sleep(1);


/*
		// read only one time
		unsigned char rxbuff[size];
		int recvsize = ML605Recv(testfd,rxbuff,4096);
	   	if(recvsize!=size)
			printf("recv error!\n");
        	else{
     			mycount = mycount + 1;
	
			printf("recv successful!=%d",recvsize);
            		for(int i=0;i<10;i++)
	           		printf("%x,",rxbuff[i]);
            		printf("one receive cycle\n");
        	}//end else
  */
  char ch_input;
  scanf("%c", &ch_input);
  isclose=true;
  ML605Close(testfd);
}
