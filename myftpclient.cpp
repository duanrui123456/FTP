#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <pthread.h>

#define BUF_SIZE 512

using namespace std;

const char FSEND[] = "File Send";
const char CONFIRM[] = "Done";
const char ERROR[] = "Error Occurred";
const char ABORT[] = "File Transfer Terminated";
const char INVALCMD[] = "Invalid Command";

int put(const int *socket, const string *cmd, bool *terminate);
void get(const int *socket);
void *new_thread(void * thrd_info_struct);

struct thrd_info
{
  int *socket;
  pthread_t *thrd_id;
  string command;
  bool *is_terminate;
};

int main(int argc, char* argv[])
{
  // check for com line args
  if(argc != 4)
  {
    cout << "myftp requires two command line arguments: the machine name where the server resides, the port number to connect to and a terminate port number." << endl;
    exit(-1);
  }
  
  // retrieve server location
  in_addr_t server_ip = inet_addr(argv[1]);
  
  // note: inet_addr() returns -1 on error, but in_addr_t is unsigned, so -1 signed = 4294967295 unsigned
  if(server_ip == 4294967295)
  {
    cout << argv[1] << " is not a valid ip address." << endl;
    exit(-1);
  }

  // retrieve port number and terminate port number to listen on
  int port_num = atoi(argv[2]);
  int term_port_num = atoi(argv[3]);

  // bad port number, an atoi() error (-1) is inherently handled here
  if(port_num < 1 || port_num > 65535)
  {
    cout << argv[2] << " is not a valid port number." << endl;
    exit(-1);
  }
  if(term_port_num < 1 || term_port_num > 65535)
  {
    cout << argv[3] << " is not a valid terminate port number." << endl;
    exit(-1);
  }
  if(port_num == term_port_num)
  {
    cout << "Port number " << argv[2] << " and terminate port number " << argv[3] << " cannot be the same." << endl;
    exit(-1);
  }

  int tcp_socket = socket(AF_INET,SOCK_STREAM,0);
  if(tcp_socket < 0)
  {
    perror("cannot create socket");
    return 0;
  }

  struct sockaddr_in clientaddr;
  clientaddr.sin_family = AF_INET;
  clientaddr.sin_addr.s_addr = server_ip;
  clientaddr.sin_port = htons(port_num);
  
  connect(tcp_socket,(struct sockaddr *)&clientaddr,sizeof(struct sockaddr));

  bool is_terminate = false;
  // keep accepting commands until user enters quit
  for(;;)
  {
    char message[BUF_SIZE];
    string command = "";
    printf("myftp> ");
    getline(cin, command);

    if(command.substr(0,9) == "terminate")
    {
      // create new socket for terminate cmd
      int term_socket = socket(AF_INET,SOCK_STREAM,0);
      if(term_socket < 0)
      {
        perror("cannot create terminate socket");
        continue;
      }

      struct sockaddr_in term_clientaddr;
      term_clientaddr.sin_family = AF_INET;
      term_clientaddr.sin_addr.s_addr = server_ip;
      term_clientaddr.sin_port = htons(term_port_num);

      connect(term_socket,(struct sockaddr *)&term_clientaddr,sizeof(struct sockaddr));
      // send server terminate ID
      send(term_socket,command.substr(10).c_str(),BUF_SIZE,0);
      recv(term_socket,message,BUF_SIZE,0);
      // receive error IDs
      if((strcmp(message, INVALCMD)) == 0)
      {
        printf("%s\n",INVALCMD);
        close(term_socket);
        continue;
      }
      
      is_terminate = true;
      cout << "Transfer terminated" << endl;
      close(term_socket);
      continue;
    }

    // inputting nothing crashes server
    else if(command == "")
    {
      continue;
    }

    else if(command.compare(command.length()-1,1,"&") == 0)
    {
      int *socket = &tcp_socket;
      pthread_t *thread = new pthread_t;

      struct thrd_info *this_thrd_info = new struct thrd_info;

      this_thrd_info->socket = socket;
      this_thrd_info->thrd_id = thread;
      this_thrd_info->command = command;
      this_thrd_info->is_terminate = &is_terminate;

      if(pthread_create(thread, NULL, new_thread, this_thrd_info) != 0)
      {
        cout << "Thread could no be created." << endl;
        continue;
      }
      continue;
    }
    else if(command.substr(0,3) == "put")
    {
      if(put(&tcp_socket, &command, &is_terminate) == 0)
      {
        // print error if file does not exist
        printf("%s\n",ERROR);
        continue;
      }
      else
      {
        continue;
      }
    }

    else
    {
      send(tcp_socket,command.c_str(),BUF_SIZE,0);
    
      if(command == "quit")
      {
        break;
      }
      
      recv(tcp_socket,message,BUF_SIZE,0);

      // don't print CONFIRM string
      if((strcmp(message,CONFIRM)) == 0)
      {
        continue;
      }
      
      // receive file
      if((strcmp(message, FSEND)) == 0)
      {
        get(&tcp_socket);
      }
      else
      {
        printf("%s\n",message);
      }
    }
  }
  
  close(tcp_socket);
}// main

void get(const int *socket)
{
  int file_size = 0;
  char message[BUF_SIZE+1];
  // receive ID
  recv(*socket,message,BUF_SIZE,0);
  const string ID = message;
  cout << "get ID: " << ID << endl;
  // receive file name
  recv(*socket,message,BUF_SIZE,0);
  FILE *file = fopen(basename(message), "w");
  const string file_name = message;

  // keep receiving file until it CONFIRM is received
  while(file_size = recv(*socket,message,BUF_SIZE+1,0))
  {  
    fwrite(message,sizeof(char),file_size,file);

    // if get recieves ABORT string; clean up and quit
    if((strcmp(message,ABORT)) == 0)
    {
      fclose(file);
      remove(basename(file_name.c_str()));
      return;
    }// if

    // server is done sending
    if(file_size <= BUF_SIZE)
    {
      break;
    }
  }
  fclose(file);
}// get

int put(const int *socket, const string *cmd, bool *terminate)
{
  int file_size = 0;
  char read_file[BUF_SIZE+1];
  char message[BUF_SIZE];
  FILE *file = fopen(cmd->substr(4,cmd->length()-1).c_str(), "r");
  if(file == NULL)
  {
    return 0;
  }
  else
  {
    // send command
    send(*socket,cmd->c_str(),BUF_SIZE,0);
    
    // receive ID
    recv(*socket,message,BUF_SIZE,0);
    const char *ID = message;
    cout << "put ID: " << ID << endl;
    // keep sending file until end of file
    while(file_size = fread(read_file, sizeof(char), BUF_SIZE+1, file))
    {
      send(*socket,read_file,file_size,0);
      memset(read_file,'\0',BUF_SIZE);
      // end of file reached
      if(file_size == 0)
      {
        break;
      }
      if(*terminate)
      {
        *terminate = false;
        break;
      }
    }
    fclose(file);
  }
  return 1;
}// put

void *new_thread(void * thrd_info_struct)
{
  char message[BUF_SIZE];
  struct thrd_info *this_thrd_info = (struct thrd_info*) thrd_info_struct;
  
  if(this_thrd_info->command.substr(0,3) == "put")
  {
    // string file removes the "&" before sending to server
    string file = this_thrd_info->command.substr(0,this_thrd_info->command.length()-2);
  
    if(put(this_thrd_info->socket,&file,this_thrd_info->is_terminate) == 0)
    {
      // print error if file does not exist
      printf("%s\n",ERROR);
    }
    pthread_cancel(*(this_thrd_info->thrd_id));
  }
  else
  {
    // send command
    send(*(this_thrd_info->socket),this_thrd_info->command.c_str(),BUF_SIZE,0);
    recv(*(this_thrd_info->socket),message,BUF_SIZE,0);
    
    // don't print CONFIRM string
    if((strcmp(message,CONFIRM)) == 0)
    {
    
    }
    
    // receive file
    if((strcmp(message, FSEND)) == 0)
    {
      get(this_thrd_info->socket);
    }
    else
    {
      printf("%s\n",message);
    }
  }
}// new_thread
