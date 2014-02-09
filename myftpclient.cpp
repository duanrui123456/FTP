#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iostream>

#define PORTNUM 3019
#define BUF_SIZE 512

using namespace std;

int main(int argc, char* argv[])
{
  int tcp_socket = socket(AF_INET,SOCK_STREAM,0);
  if(tcp_socket < 0)
  {
    perror("cannot create socket");
    return 0;
  }
  struct sockaddr_in clientaddr;
  
  clientaddr.sin_family = AF_INET;
  clientaddr.sin_addr.s_addr = inet_addr("127.0.0.1");
  clientaddr.sin_port = htons(PORTNUM);
  
  connect(tcp_socket,(struct sockaddr *)&clientaddr,sizeof(struct sockaddr));

  const char FSEND[] = "File Send";
  const char CONFIRM[] = "Done";

  // keep accepting commands until user enters quit
  for(;;)
  {
    string command = "";
    printf("myftp>%c",' ');
    getline(cin, command);
    // inputing nothing crashes server
    if(command == "")
      continue;

    // TODO make sure command consists of no more than two tokens

    send(tcp_socket,command.c_str(),BUF_SIZE,0);
    if(command == "quit")
    {
      break;
    }// if
    
    char message[BUF_SIZE];
    recv(tcp_socket,message,BUF_SIZE,0);
    if((strcmp(message, FSEND)) == 0)
    {
      // receive file name
      recv(tcp_socket,message,BUF_SIZE,0);
      int file_size = 0;
      char read_file[BUF_SIZE];
      FILE *file = fopen(message, "a+");

      // keep receiving file until it reaches end
      while(recv(tcp_socket,message,BUF_SIZE,0))
      {
        if((strcmp(message,CONFIRM)) == 0)
        {
            break;
        }// if
        fwrite(message,sizeof(char),BUF_SIZE,file);
      }// while
      fclose(file);
    }// if
    else
    {
      printf("%s\n",message);
    }// else
  }// for
  
  close(tcp_socket);
}// main