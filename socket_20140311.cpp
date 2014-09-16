
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


int sock_fd, recvbytes;
int sock_fd2, recvbytes2;
int sock_fd3, recvbytes3;


struct hostent *host;
struct sockaddr_in serv_addr;
struct sockaddr_in serv_addr2;
struct sockaddr_in serv_addr3;



int socket_init(){
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

    return 0;
}


  

void socket_send(){
    sendto(sock_fd,send_buff,14404,0,(struct sockaddr *)&serv_addr,sizeof(serv_addr));//port:7005 , send to QPSK plot GUI 
    sendto(sock_fd2,send_buff,14404,0,(struct sockaddr *)&serv_addr2,sizeof(serv_addr));//port:7004, send to channel plot GUI
    sendto(sock_fd3,send_buff,14404,0,(struct sockaddr *)&serv_addr3,sizeof(serv_addr));//port:7003, send to channel plot GUI
}