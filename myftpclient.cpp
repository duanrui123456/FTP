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
  
  // keep accepting commands until user enters quit
  for(;;)
  {
    string command = "";
    cout << "myftp> ";
    getline(cin, command);


    // TODO make sure command consists of no more than two tokens


    send(tcp_socket,command.c_str(),BUF_SIZE,0);
    if(command == "quit")
    {
      break;
    }// if
    
    char message[BUF_SIZE];
    recv(tcp_socket,message,BUF_SIZE,0);
    cout << message << endl;
  }// for
  
  close(tcp_socket);
}
