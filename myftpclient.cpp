#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iostream>

#define BUF_SIZE 512

using namespace std;

int main(int argc, char* argv[])
{
  // check for com line args
  if(argc != 3)
  {
    cout << "myftp requires two command line arguments: the machine name where the server resides and the port number to connect to." << endl;
    exit(-1);
  }// if
  
  // retrieve server location
  in_addr_t server_ip = inet_addr(argv[1]);

  // call the user dumb for bad input
  // note: inet_addr() returns -1 on error, but in_addr_t is unsigned, so -1 signed = 4294967295 unsigned
  if(server_ip == 4294967295)
  {
    cout << argv[1] << " is not a valid ip address." << endl;
    exit(-1);
  }// if

  // retrieve port number to listen on
  int port_num = atoi(argv[2]);

  // tell the user to screw off if he enters a bad port number, an atoi() error (-1) is inherently handled here
  if(port_num < 1 || port_num > 65535)
  {
    cout << argv[2] << " is not a valid port number." << endl;
    exit(-1);
  }// if

  int tcp_socket = socket(AF_INET,SOCK_STREAM,0);
  if(tcp_socket < 0)
  {
    perror("cannot create socket");
    return 0;
  }
  struct sockaddr_in clientaddr;
  
  clientaddr.sin_family = AF_INET;
  clientaddr.sin_addr.s_addr = server_ip; //inet_addr("127.0.0.1");
  clientaddr.sin_port = htons(port_num);
  
  connect(tcp_socket,(struct sockaddr *)&clientaddr,sizeof(struct sockaddr));

  const char FSEND[] = "File Send";
  const char CONFIRM[] = "Done";

  // keep accepting commands until user enters quit
  for(;;)
  {
    string command = "";
    printf("myftp>%c",' ');
    getline(cin, command);
    
    // inputting nothing crashes server
    if(command == "")
    {
      continue;
    }// if

    send(tcp_socket,command.c_str(),BUF_SIZE,0);
    if(command == "quit")
    {
      break;
    }// if
    
    char message[BUF_SIZE];
    recv(tcp_socket,message,BUF_SIZE,0);

    // don't print CONFIRM string
    if((strcmp(message,CONFIRM)) == 0)
    {
      continue;
    }// if
    
    // receive file
    if((strcmp(message, FSEND)) == 0)
    {
      // receive file name
      recv(tcp_socket,message,BUF_SIZE,0);
      FILE *file = fopen(message, "w");

      // keep receiving file until it reaches end
      while(recv(tcp_socket,message,BUF_SIZE,0))
      {
        if((strcmp(message,CONFIRM)) == 0)
        {
            break;
        }// if
        else
        {
          int char_count = BUF_SIZE;
          while(message[char_count-1] == '\0')
          {
            char_count--;
          }// while
          fwrite(message,sizeof(char),char_count,file);
        }// else
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
