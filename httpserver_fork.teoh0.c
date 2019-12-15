#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>


#define LISTENQ 10
#define MAXLINE 1024

int open_listenfd(int port)  
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
    //encrypted(buf, atoi(shift));
    write(connfd, buf, strlen(buf));//print encrypted file content
    bzero(buf, MAXLINE);
  }
}

//permission for testing: chmod u+x multi_service_client
/*testing command:
chmod 000 httpResponse.txt
./shift_client localhost 2100 /test_short.txt 4 > output1.txt
diff output1.txt short_encrypted_4.txt
./shift_client localhost 2100 /test_very_long.txt 7 > output1.txt
diff output1.txt short_encrypted_4.txt
*/
//compile command: gcc -o httpserver_fork httpserver_fork.teoh0.c
//invoke command: ./httpserver_fork 2100
//testing fork: ps -ef | grep httpserver_fork
//debug: gcc -g -Wall httpserver_fork.teoh0.c -o httpserver_fork
int main(int argc, char **argv) {
  int listenfd, connfd, port, childpid;
  socklen_t clientlen;
  struct sockaddr_in clientaddr;
  struct hostent *hp;
  char *haddrp;

  port = atoi(argv[1]);//server listens on a port passed on the command line
  listenfd = open_listenfd(port); //open connection to client

  while (1) {
    clientlen = sizeof(clientaddr); 
    connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &clientlen);

    if ((childpid = fork()) == 0){//fork == 0 means child completed process
      hp = gethostbyaddr((const char *)&clientaddr.sin_addr.s_addr,//retrieve info from client
		       sizeof(clientaddr.sin_addr.s_addr), AF_INET);
      haddrp = inet_ntoa(clientaddr.sin_addr);//client address string pointer
      process(connfd);
      close(listenfd);
      exit(0);
    }
    close(connfd);
  }
  close(listenfd);
  return 0;
}

