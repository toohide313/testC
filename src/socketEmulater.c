#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <pthread.h>


#define DEFAULT_IP   "127.0.0.1"
#define DEFAULT_PORT 8080
#define BUF_LEN 1024

char *logID = "  ";

typedef struct {
  int newfd;
  int number;
} MY_THREAD_ARG;

void printUsage(char*);
int client_func(char*,char*);
int server_func(char*,char*);
char *getTime(void);
void *clThread(void*);
void *svThread(void*);
void plog(int,char*, ...);

void printUsage(char *pname){
  fprintf(stdout,
    "usage: %s <sv|cl> [server ip address(def:127.0.0.1)]  [server portnumber(def:8080)]\n",pname);
  exit(1);
}

int main(int argc, char *argv[]){
  
  char *ip = DEFAULT_IP;
  char *sw;
  char *port = DEFAULT_PORT;
  
  if ( argc != 4 ){
     printUsage(argv[0]);
  }
  
  sw   = argv[1];
  ip   = argv[2];
  port = argv[3];
  
  if( strcmp(sw,"cl") == 0){
    logID = "CL";
    plog(0,"mode client ip:%s port:%s",ip,port);
    client_func(ip,port);
  }else if( strcmp(sw,"sv") == 0 ){
    logID = "SV";
    plog(0,"mode server ip:%s port:%s",ip,port);
    server_func(ip,port);
  }else{
     printUsage(argv[0]);
  }
  
  return 0;
}


int client_func(char* ip,char* port){

    int sockfd;
    pthread_t thread_a;
    MY_THREAD_ARG thread_a_arg;
    int status;
    int ret,num = 0;
    char c;

    sockfd = client_connect_init(ip,port);

    while((c = getchar()) != EOF){

      switch(c){
        case 'c':
	   close(sockfd);
           return 0;
        default:
          thread_a_arg.newfd = sockfd;
          thread_a_arg.number = ++num;
          status = pthread_create(&thread_a, NULL, clThread, &thread_a_arg);

          if(status!=0){
            plog(num,"pthread_create(%d) failed",status);
            exit(1);
          }
          pthread_detach( thread_a );

      }
    }
    plog(0,"end?");
    close(sockfd);
    return 0;
}


int server_func(char* ip,char* port){

   int sockfd;
   struct sockaddr_in server;

   MY_THREAD_ARG thread_a_arg;
   int status;
   int num;

   sockfd = server_connect_init(ip,port);

   // メインループ
   while (1) {
      struct sockaddr_in client;
      int newfd,len;

      pthread_t thread_a;

      // clientからの接続を受け付ける
      memset(&client, 0, sizeof(client));
      len = sizeof(client);
      plog(0,"accept() start");
      if (( newfd = accept(sockfd, (struct sockaddr *) &client, &len)) < 0) {
         printf("accept error");
         exit(1);
      }
      plog(0,"accept() end");

      thread_a_arg.newfd = newfd;
      thread_a_arg.number = ++num;

      status=pthread_create(&thread_a, NULL, svThread, &thread_a_arg);

      if(status!=0){
         plog(num,"pthread_create(%d) failed",status);
         exit(1);
      }
   }
}

int client_connect_init(char* ip,char *port){

    int sockfd;

    struct hostent *servhost;
    struct sockaddr_in server;

    bzero(&server, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_port = htons(atoi(port));

    plog(0,"socket() start");
    if (( sockfd = socket(AF_INET,SOCK_STREAM,0)) < 0 ){
        plog(0,"[CL]socket() error");
        exit(1);
    }
    plog(0,"socket() end");

    plog(0,"connect() start");
    if ( connect(sockfd, (struct sockaddr *)&server, sizeof(server)) == -1 ){
        plog(0,"connect() error");
        exit(1);
    }
    plog(0,"connect() end");

  return sockfd;
}


int server_connect_init(char* ip,char *port){

   int sockfd;
   struct sockaddr_in server;

   if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      error("socket error");
      exit(1);
   }
   
   // Port, IPアドレスを設定(IPv4)
   memset(&server, 0, sizeof(server));
   server.sin_family = AF_INET;   
   server.sin_addr.s_addr = INADDR_ANY;
   server.sin_port = htons(atoi(port));
   
   // ディスクリプタとPortを結び付ける
   plog(0,"bind() start");
   if (bind(sockfd, (struct sockaddr *) &server, sizeof(server)) < 0) {
      perror("bind error");
      exit(1);
   }
   plog(0,"bind() start");
   
   // listen準備
   plog(0,"listen() start");
   if (listen(sockfd, SOMAXCONN) < 0) {
      perror("listen error");
      exit(1);
   }
   plog(0,"listen() end");

  return sockfd;
}


void *clThread(void *arg){
   char buf[BUF_LEN] = "";
   char c;
   int size;
   int num;
   int sockfd,ret;
   char* sent_str;

   sent_str = (char*)malloc(sizeof(char)*BUF_LEN);
   memset(sent_str, 0x1 , BUF_LEN);

   MY_THREAD_ARG *my_thread_arg =(MY_THREAD_ARG*)arg;
   sockfd = my_thread_arg->newfd;
   num = my_thread_arg->number;

   plog(num,"thread start");

   plog(num,"write() start");
   ret = write(sockfd, sent_str, strlen(sent_str));

   if( ret >= 0){
     plog(num,"write(size:%d/%s) ) end",ret,strerror( errno ));
   }else{
     plog(num,"write(size:%d/%s) end",ret,strerror( errno ) );
   }
   plog(num,"thread end",num);

   return NULL;
}


void *svThread(void *arg){
   char *buf;
   char c;
   int sockfd;
   int size;
   int num;

   buf = (char*)malloc(sizeof(char)*BUF_LEN);

   MY_THREAD_ARG *my_thread_arg =(MY_THREAD_ARG*)arg;
   sockfd = my_thread_arg->newfd;
   num = my_thread_arg->number;

   plog(num,"thread start");

   while((c = getchar()) != EOF){

     switch(c){
       case 'c':
         plog(num,"close(%d)",my_thread_arg->newfd);
         close(my_thread_arg->newfd);
         plog(num,"thread end");
         return NULL;
       default:
         plog(num,"read() start");
         size = read(sockfd, buf,BUF_LEN); 
         if ( size == -1 ){
           close(sockfd);
           plog(num,"read(size:%d) error(%d)",size,errno);
           plog(num,"thread end",num);
           return NULL;
         }
         plog(num,"read(size:%d/errno:%d) end",size,errno);
     }
   }
   send(my_thread_arg->newfd, "OK", strlen("OK"), 0);
   close(my_thread_arg->newfd);
   plog(num,"thread end");
   return NULL;
}


void plog(int num,char* str, ...){
  va_list va;

   struct timeval myTime;
   struct tm *time_st;

   gettimeofday(&myTime, NULL);
   time_st = localtime(&myTime.tv_sec);

  printf("%4d/%02d/%02d %02d:%02d:%02d.%06d [%s:%05d]: ",
      time_st->tm_year+1900, 
      time_st->tm_mon+1,
      time_st->tm_mday,
      time_st->tm_hour,
      time_st->tm_min,
      time_st->tm_sec,
      myTime.tv_usec,
      logID,num
  );
  va_start(va,str);
  vprintf(str,va);
  va_end(va);
  printf("\n");
}
