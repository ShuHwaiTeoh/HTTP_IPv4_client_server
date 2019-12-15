#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

#define MAXSIZE 1024

int open_clientfd(char *hostname, int port) 
{ 
  int clientfd; 
  struct hostent *hp; //entry of host database
  struct sockaddr_in serveraddr; //store address of internet protocol family
 
  if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;//IPv4, TCP
 
  /* Fill in the server's IP address and port (host name to IP+port*/ 
  if ((hp = gethostbyname(hostname)) == NULL) return -2;
  bzero((char *) &serveraddr, sizeof(serveraddr));//fill with zeros
  serveraddr.sin_family = AF_INET; //IPv4
  bcopy((char *)hp->h_addr,(char *)&serveraddr.sin_addr.s_addr, hp->h_length);//copy h_addr to s_addr 
  serveraddr.sin_port = htons(port);//convert host byte order to network byte order

  /* Establish a connection with the server */ 
  if (connect(clientfd, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0) return -1; 
  return clientfd; 
} 

void GET(int clientfd, char *path){
  char requestBuf[MAXSIZE] = {0};
  
  //Send formatted output to requestBuf as a request
  sprintf(requestBuf, "GET %s HTTP/1.0\r\n\r\n", path);
  write(clientfd, requestBuf, strlen(requestBuf));//Send a message on socket clientfd
}

char* extract(char *buf){
  int i, index = 2, j=0;

  //find the start index of ile path in HTTP response
  for (i=strlen(buf)-2; buf[i]!='\n'; i--){
    index++;
  }
  //copy the file path to file pointer
  char *file=malloc (sizeof(char)*index);
  for (i=strlen(buf)-index+1; buf[i]!='\n'; i++){
    file[j++]=buf[i];
  }

  return file;
}

//compile command: gcc -o httpclient httpclient.teoh0.c
//invoke command: ./httpclient dtunes.ecn.purdue.edu 80 /ece463/lab1/path_very_long.txt > output1.txt
//test: diff output1.txt test_very_long.txt
//./httpclient www.w3.org 80 /TR/PNG/iso_8859-1.txt  > output2.txt
//nslookup dtunes.ecn.purdue.edu: 128.46.76.106
int main(int argc, char **argv)
{ 
  int clientfd, port, flag=0; 
  char *host, buf[MAXSIZE], *path, *file=NULL;

  if (argc != 4){
    fprintf(stderr, "Command line arguments: ./httpclient <servername> <serverport> <pathname>\n");
    exit(1);
  }
  host = argv[1]; 
  port = atoi(argv[2]);
  path = argv[3];
  
  //Establishes a connection to server
  clientfd = open_clientfd(host, port);
  if (clientfd < 0){
    fprintf(stderr, "[main51]: clientfd < 0, fail to connect.\n");
    exit(1);
  }

  //Sends Get command to server
  GET(clientfd, path);
  
  //Prints the server response
  bzero(buf, MAXSIZE);
  while(read(clientfd, buf, MAXSIZE-1) > 0){
    if ((strstr(buf, "200 OK")==NULL) && (flag==0)) {
      fprintf(stdout, "%s", buf);//print response from server
      exit(1);//check status code of response is 200
    }
    flag=1;
    fprintf(stdout, "%s", buf);//print response from server
    if (strstr(buf, "\r\n\r\n/")!=NULL) {file = extract(buf);}//extrac file path from response
    bzero(buf, MAXSIZE);
  }
  if (file==NULL) exit(1);
  //Establishes a connection to server
  clientfd = open_clientfd(host, port);

  //Send second Get request to server
  GET(clientfd, file);

  //Prints server response
  bzero(buf, MAXSIZE);
  flag=0;
  while(read(clientfd, buf, MAXSIZE-1) > 0){
    if ((strstr(buf, "200 OK")==NULL) && (flag==0)) {
      fprintf(stdout, "%s", buf);//print response from server
      exit(1);//check status code of response is 200
    }
    flag=1;
    fprintf(stdout, "%s", buf);
    bzero(buf, MAXSIZE);
  }

  free(file);
  close(clientfd);
  exit(0);
} 



