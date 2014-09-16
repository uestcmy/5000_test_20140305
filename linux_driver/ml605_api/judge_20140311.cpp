#include <stdio.h>

int judge_header_a0aa(unsigned char *rxbuff){

    if((rxbuff[0] == 0xaa && rxbuff[1] == 0xa0 && rxbuff[2] == 0xaa && rxbuff[3] == 0xa0 )||
       (rxbuff[0] == 0xa0 && rxbuff[1] == 0xaa && rxbuff[2] == 0xa0 && rxbuff[3] == 0xaa )
    ){
         return 1;
    }else{
    	return 0;
    }	
}

int judge_tail_a0aa(unsigned char *rxbuff,int size){
    if(
    	(rxbuff[size-2] != 0xaa && rxbuff[size-1] != 0xa0 ) && ( rxbuff[size-2] != 0xa0 && rxbuff[size-1] != 0xaa)
    ){
         return 1;
    }else{
    	return 0;
    }

}
