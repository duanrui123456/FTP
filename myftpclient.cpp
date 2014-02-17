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

const char FSEND[] = "File Send";
const char CONFIRM[] = "Done";
const char ERROR[] = "Error Occurred";

int put(const int *socket, const string *cmd);
void get(const int *socket);

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

  // keep accepting commands until user enters quit
  for(;;)
  {
    char message[BUF_SIZE];
    string command = "";
    printf("myftp>%c",' ');
    getline(cin, command);
    
    // inputting nothing crashes server
    if(command == "")
    {
      continue;
    }// if

    if(command.substr(0,3) == "put")
    {
      if(put(&tcp_socket,&command) == 0)
      {
        // print error if file does not exist
        printf("%s\n",ERROR);
        continue;
      }// if
      else
      {
        continue;
      }
    }

    send(tcp_socket,command.c_str(),BUF_SIZE,0);

    if(command == "quit")
    {
      break;
    }// if
    
    recv(tcp_socket,message,BUF_SIZE,0);

    // don't print CONFIRM string
    if((strcmp(message,CONFIRM)) == 0)
    {
      continue;
    }// if
    
    // receive file
    if((strcmp(message, FSEND)) == 0)
    {
      get(&tcp_socket);
    }// if
    else
    {
      printf("%s\n",message);
    }// else
  }// for
  
  close(tcp_socket);
}// main

void get(const int *socket)
{
  int file_size = 0;
  char message[BUF_SIZE+1];
  // receive file name
  recv(*socket,message,BUF_SIZE,0);
  FILE *file = fopen(basename(message), "w");

  // print to console
  printf("Fetching %s\n",message);

  // keep receiving file until it CONFIRM is received
  while(file_size = recv(*socket,message,BUF_SIZE+1,0))
  {  
    fwrite(message,sizeof(char),file_size,file);
    // server is done sending
    if(file_size <= BUF_SIZE)
    {
      break;
    }// if
  }// while
  fclose(file);
}// get

int put(const int *socket, const string *cmd)
{
  int file_size = 0;
  char read_file[BUF_SIZE+1];
  FILE *file = fopen(cmd->substr(4,cmd->length()-1).c_str(), "r");
  if(file == NULL)
  {
    return 0;
  }// if
  else
  {
    // print to console
    printf("Uploading %s\n",cmd->substr(4,cmd->length()-1).c_str());
    // send command
    send(*socket,cmd->c_str(),BUF_SIZE,0);
    // keep sending file until end of file
    while(file_size = fread(read_file, sizeof(char), BUF_SIZE+1, file))
    {
      send(*socket,read_file,file_size,0);
      memset(read_file,'\0',BUF_SIZE);
      // end of file reached
      if(file_size == 0)
      {
        break;
      }// if
    }// while
    fclose(file);
  }// else
  return 1;
}// put