#include <stdio.h>
#include <stdlib.h>

int main(){
	freopen("dataout.txt","r",stdin);
	for( int i = 0 ; i < 100 ; i++ ){
		char tmp;
		scanf("%c",&tmp);
		printf("%c",tmp);
	}
	return 0;
}
