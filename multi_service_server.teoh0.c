#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <netinet/ip.h>


#define LISTENQ 10
#define MAXLINE 1024
#define PINGSIZE 128

int open_tcpfd(int port)  
{ 
  int listenfd, optval=1; 
  struct sockaddr_in serveraddr; 
   
  /* Create a socket descriptor */ 
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1; 
  
  /* Eliminates "Address already in use" error from bind.*/
  /* set options on socket, tells kernel even if this port is busy (in TIME_WAIT sate),
  reuse it. If it is busy with another state, get in use error*/
  /*optval: a pointer points to a buffer containing data needed by the set command*/
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
		 (const void *)&optval , sizeof(int)) < 0) return -1; 
 
  /* Listenfd will be an endpoint for all requests to port on any IP address for this host */ 
  bzero((char *) &serveraddr, sizeof(serveraddr)); 
  serveraddr.sin_family = AF_INET;//IPv4  
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);//socket listen on all available interface  
  serveraddr.sin_port = htons((unsigned short)port);
  //socket created exists in a name space (address family), no addresss assigned to it
  //bind() assigns address specified by serveraddr to the socket(listenfd)
  if (bind(listenfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) return -1; 
 
  /* Make it a listening (passive) socket ready to accept connection requests */
  //LISTENQ: MAX length of pending connections on queue
  if (listen(listenfd, LISTENQ) < 0) return -1; 
 
  return listenfd; 
}

int open_udpfd(int port)  
{ 
  int udpfd, optval=1; 
  struct sockaddr_in serveraddr; 
   
  /* Create a socket descriptor */ 
  if ((udpfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) return -1; 
  
  /* Eliminates "Address already in use" error from bind.*/
  /* set options on socket, tells kernel even if this port is busy (in TIME_WAIT sate),
  reuse it. If it is busy with another state, get in use error*/
  /*optval: a pointer points to a buffer containing data needed by the set command*/
  if (setsockopt(udpfd, SOL_SOCKET, SO_REUSEADDR, 
		 (const void *)&optval , sizeof(int)) < 0) return -1; 
 
  /* Listenfd will be an endpoint for all requests to port on any IP address for this host */ 
  bzero((char *) &serveraddr, sizeof(serveraddr)); 
  serveraddr.sin_family = AF_INET;//IPv4  
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);//socket listen on all available interface  
  serveraddr.sin_port = htons((unsigned short)port);
  //socket created exists in a name space (address family), no addresss assigned to it
  //bind() assigns address specified by serveraddr to the socket(listenfd)
  if (bind(udpfd, (struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) return -1; 
 
  /* Make it a listening (passive) socket ready to accept connection requests */
  //LISTENQ: MAX length of pending connections on queue
  //if (listen(listenfd, LISTENQ) < 0) return -1; 
 
  return udpfd; 
}

void header(int connfd, int status){
  char header[MAXLINE] = {0};

  switch (status){
    case 2: sprintf(header,"HTTP/1.0 404 Not Found\r\n\r\n");
      break;
    case 3: sprintf(header, "HTTP/1.0 403 Forbidden\r\n\r\n");
      break;
    default: sprintf(header, "HTTP/1.0 200 OK\r\n\r\n");
      break;
  }
  write(connfd, header, strlen(header));
}

void process(int connfd){ 
  char buf[MAXLINE];
  char *command;
  char *filePath;
  char unixFile[MAXLINE] = {0};
  char *shift;
  int i, shiftNumber=0;

  //(GET <filepath> <shiftnumber> HTTP/1.0<CR><LF><CR><LF>
  read(connfd, buf, MAXLINE);//read command line from client 
  command = strtok(buf, " ");//parse command line by " "
  if (strcmp(command, "GET") != 0) return;//ignore other HTTP commands

  filePath = strtok(NULL, " ");//extract file path from command line
  strcpy(unixFile, ".");
  strcat(unixFile, filePath);//"/index.html" to "./index.html"

  if (filePath[0] == '/'){
    strcpy(unixFile, ".");
    strcat(unixFile, filePath);//"/index.html" to "./index.html"
  }
  else strcpy(unixFile, filePath);
  
  shift = strtok(NULL, " ");//extractshift number from command line
  shiftNumber = atoi(shift);

  if (access(unixFile, F_OK) != 0){//file does not exist
    header(connfd, 2);//print 404 header
    return;
  }
  else if (access(unixFile, R_OK) != 0){//file cannot read
    header(connfd, 3);//print 403 header
    return;
  }
  else {
    header(connfd, 1);//print 200 header
  }

  FILE *file = fopen(unixFile, "r");
  bzero(buf, MAXLINE);
  while(fgets(buf, MAXLINE, file)){
    //encrypted file content by shift
    for(i = 0; i < strlen(buf); i++){
      if (buf[i] >= 65 && buf[i] <= 90){
        buf[i] = (buf[i] - shiftNumber);
        if (buf[i] < 65) buf[i] += 26;
      }
      else if (buf[i] >= 97 && buf[i] <= 122){
        buf[i] = (buf[i] - shiftNumber);
        if (buf[i] < 97) buf[i] += 26;
      }
    }

    write(connfd, buf, strlen(buf));//print encrypted file content
    bzero(buf, MAXLINE);
  }
}

//permission for testing: chmod 777 multi_service_client
/*testing command:
chmod 000 httpResponse.txt
./multi_service_client localhost 2100 /test_short.txt 2200 4 
./multi_service_client localhost 2100 /test_very_long.txt 2200 7
*/
//compile command: gcc -o multi_service_server multi_service_server.teoh0.c
//invoke command: ./multi_service_server 2100 2200
//debug: gcc -g -Wall multi_service_server.teoh0.c -o multi_service_server
int main(int argc, char **argv) {
  int listenfd, connfd, port, childpid, udpfd, retval, udpRecSize, i, IPbyte;
  socklen_t clientlen;
  struct sockaddr_in tcpClientAddr, udpClientAddr;
  struct hostent *hp, *udphp;
  char *haddrp;//, *udphaddrp;
  fd_set socketSet;
  struct in_addr udpClientInfo;

  port = atoi(argv[1]);//server listens on a port passed on the command line
  listenfd = open_tcpfd(port); //open connection to client

  port = atoi(argv[2]);//server listens on a port passed on the command line
  udpfd = open_udpfd(port); //open connection to client

  while (1) {
    FD_ZERO(&socketSet);
    FD_SET(listenfd, &socketSet);
    FD_SET(udpfd, &socketSet);

    retval = select(udpfd > listenfd ? udpfd + 1: listenfd + 1, &socketSet, NULL, NULL, NULL);

    if(FD_ISSET(udpfd, &socketSet)){
      char udpBuf[MAXLINE] = {0}, seq[PINGSIZE] = {0}, udpClientIP[PINGSIZE] = {0};//, result[PINGSIZE] = {0};
      clientlen = sizeof(udpClientAddr); 
      udpRecSize = recvfrom(udpfd, udpBuf, sizeof(udpBuf), 0, (struct sockaddr *)&udpClientAddr, &clientlen);
      
      IPbyte = udpRecSize - 4;
      for(i=0; i < IPbyte; i++) udpClientIP[i] = udpBuf[i];
      inet_pton(AF_INET, udpClientIP, &udpClientInfo);
      udphp = gethostbyaddr((const char *)&udpClientInfo, sizeof(udpClientInfo), AF_INET);

      for (i=0; i < 4; i++) seq[i] = udpBuf[IPbyte+i];
      //strcpy(result, udphp->h_name);
      //*((uint32_t *)udpBuf) = htonl(*((uint32_t *)result));
      //for (i=0; i < 4; i++) result[strlen(udphp->h_name)+i] = seq[i];
      *((uint32_t *)seq) = htonl(ntohl( *((uint32_t *)seq))+1);
      bzero(udpBuf, MAXLINE);
      strcpy(udpBuf, udphp->h_name);
      for (i=0; i < 4; i++) udpBuf[strlen(udphp->h_name)+i] = seq[i];//strlen(udphp->h_name)+4
      sendto(udpfd, udpBuf, (strlen(udpBuf)+4), 0, (struct sockaddr *)&udpClientAddr, sizeof(udpClientAddr));
    }
    else if(FD_ISSET(listenfd, &socketSet)){
      clientlen = sizeof(tcpClientAddr); 
      connfd = accept(listenfd, (struct sockaddr *)&tcpClientAddr, &clientlen);

      if ((childpid = fork()) == 0){//fork == 0 means child completed process
	hp = gethostbyaddr((const char *)&tcpClientAddr.sin_addr.s_addr,//retrieve info from client
			   sizeof(tcpClientAddr.sin_addr.s_addr), AF_INET);
	haddrp = inet_ntoa(tcpClientAddr.sin_addr);//client address string pointer
	process(connfd);
	close(listenfd);
	exit(0);
      }
      close(connfd);
    }
  }
  
  close(listenfd);
  return 0;
}
