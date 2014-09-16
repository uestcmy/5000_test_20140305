#include <string.h>
#include <stdio.h>
#include <stdlib.h>

int convert_hex2str(unsigned char symbol,char *buff2){
    unsigned char tmp = symbol;
    int bit[8] = {0};
    int cntbit = 0;
    char buff[3];
    
    
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
    //strcat(buff2,buff);
     buff2[0] = buff[0];
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
    //strcat(buff2,buff);
      buff2[1] = buff[0];
     buff2[2] = ',';
      buff2[3] = '\0';
     //printf("symbol: is %x,char is %s",symbol,buff2);
}
