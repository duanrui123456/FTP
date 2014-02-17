#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <dirent.h>
#include <string>
#include <libgen.h>
#include <pthread.h>
#include <map>

#define BUF_SIZE 512

using namespace std;

/*	these are the function we need to implement
	get----------done
	put
	delete-------done
	ls-----------done
	cd-----------done
	mkdir--------done
	pwd----------done
	quit---------done

	use threads with pthreads package
*/

const char CONFIRM[] = "Done";
const char INVALCMD[] = "Invalid Command";
const char ERROR[] = "Error Occurred";
const char FSEND[] = "File Send";

string ls();
string pwd();
void get(const int *socket, char *arg);
void put(const int *socket, char *arg);

void *new_thread(void*);

// to be passed in as the argument during pthread_create. these are passed in due to their need to be deleted at the end of the thread
struct thrd_info
{
  int *socket;
  pthread_t *thrd_id;
  struct sockaddr_in *dest_info;
};// struct thrd_info

// keeps track of the current directory for each thread using its socket number
map<int, char*> live_connections;

int main(int argc, char* argv[])
{
  // check for cmd line arg
  if(argc != 2)
  {
    cout << "myftpserver requires one command line argument: the port number to listen on." << endl;
    exit(-1);
  }// if
  
  // retrieve port number to listen on
  int port_num = atoi(argv[1]);

  // tell the user to screw off if he enters a bad port number.  atoi() error (-1) is inherently handled here.
  if(port_num < 1 || port_num > 65535)
  {
    cout << argv[1] << " is not a valid port number." << endl;
    exit(-1);
  }// if

  // get home directory
  char home_dir[BUF_SIZE];
  getcwd(home_dir, BUF_SIZE);

  int tcp_socket = socket(AF_INET,SOCK_STREAM,0);
  if(tcp_socket < 0)
  {
    perror("Cannot create socket");
  }//if
  
  struct sockaddr_in servaddr;
  socklen_t socksize = sizeof(struct sockaddr_in);
  
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port_num);
  
  bind(tcp_socket,(struct sockaddr* )&servaddr,sizeof(struct sockaddr));
  listen(tcp_socket,5);

  // accept connections forever
  for(;;)
  {
    int *connect_socket = new int;
    pthread_t *thread_id = new pthread_t;
    struct sockaddr_in *dest = new struct sockaddr_in;

    struct thrd_info *this_thrd_info = new struct thrd_info;
    this_thrd_info->socket = connect_socket;
    this_thrd_info->thrd_id = thread_id;
    this_thrd_info->dest_info = dest;

    *connect_socket = accept(tcp_socket, (struct sockaddr*) dest, &socksize);
    // printf("Incoming Transmission from %s\n",inet_ntoa(dest.sin_addr));

    live_connections[*connect_socket] = home_dir;
    pthread_create(thread_id, NULL, new_thread, this_thrd_info);
  }// for

  // program should never reach this since it should stay in the above loop forever
  close(tcp_socket);
  return EXIT_SUCCESS;
}// main

string ls()
{
  DIR *directory;
  struct dirent *reader;
  /* open current directory */
  directory = opendir("."); 
  if(directory == NULL)
  {  
    perror("Cannot open directory");
  }// if
  
  string message = "";
  while((reader = readdir(directory))!= NULL)
  {
    // print everything except directory starting with "."
    if((reader->d_name)[0] == '.')
    {
      continue;
    }// if
    else
    { 
      message += reader->d_name;
      message += "  ";
    }// else
  }// while
  closedir(directory);
  return message;
}// ls

string pwd()
{ 
  char return_msg[BUF_SIZE] = "";
  getcwd(return_msg, BUF_SIZE);
  return string(return_msg);
}// pwd

void get(const int *socket, char* arg)
{
  int file_size = 0;
  char read_file[BUF_SIZE+1];
  FILE *file = fopen(arg, "r");
  if(file == NULL)
  {
    send(*socket,ERROR,BUF_SIZE,0);
  }// if
  else
  {
    // let client know file is incoming
    send(*socket,FSEND,BUF_SIZE,0);
    // send file name
    send(*socket,arg,BUF_SIZE,0);
    // keep sending until end of file
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
}// get

void put(const int *socket, char *arg)
{
  int file_size = 0;
  char message[BUF_SIZE+1];
  FILE *file = fopen(basename(arg), "w");
  // keep receiving file until it CONFIRM is sent
  while(file_size = recv(*socket,message,BUF_SIZE+1,0))
  {
    fwrite(message,sizeof(char),file_size,file);
    // client is done sending
    if(file_size <= BUF_SIZE)
    {
      break;
    }
  }// while
  fclose(file);
}// put

void *new_thread(void * thrd_info_struct)
{
  // get info on this connection
  struct thrd_info *this_thrd_info = (struct thrd_info*) thrd_info_struct;
  
  int connect_socket = *(this_thrd_info->socket);
  
  // continue until remote user enters quit
  for(;;)
  {
    string msg_to_send = "";
    char message[BUF_SIZE];

    recv(connect_socket,message,BUF_SIZE,0);
    
    // split message into two parts
    char *cmd = strtok(message, " ");
    char *arg = strtok(NULL, " ");
    
    // switch this thread to its current directory
    chdir(live_connections[connect_socket]);
    
    if(strcmp(cmd, "get") == 0)
    {
      get(&connect_socket,arg);
    } // if
    else if(strcmp(cmd, "put") == 0)
    {
      // get file name
      put(&connect_socket,arg);
    }// else if
    else if(strcmp(cmd, "delete") == 0)
    {
      if(remove(arg) != 0)
      {
	// send error message upon fail
	send(connect_socket,ERROR,BUF_SIZE,0);
      }// if
      else
      {  
	msg_to_send = "Removing ";
	send(connect_socket,msg_to_send.append(arg).c_str(),BUF_SIZE,0);
      }// else
    }// else if
    else if(strcmp(cmd, "ls") == 0) 
    {
      msg_to_send = ls();
      send(connect_socket,msg_to_send.c_str(),BUF_SIZE,0);
    }// else if
    else if(strcmp(cmd, "cd") == 0)
    {
      if(chdir(arg) != 0)
      {
	// send error message upon fail
	send(connect_socket,ERROR,BUF_SIZE,0);
      }// if
      else
      { 
	send(connect_socket,CONFIRM,BUF_SIZE,0);
	
	char current_dir[BUF_SIZE];
	memset(current_dir, 0, BUF_SIZE);
	getcwd(current_dir, BUF_SIZE);
	live_connections[connect_socket] = current_dir;
      }//else
    }// else if
    else if(strcmp(cmd, "mkdir") == 0)
    {
      if(mkdir(arg, 0755) != 0)
      {
	// send error message upon fail
	send(connect_socket,ERROR,BUF_SIZE,0);
      }// if
      else
      {
	send(connect_socket,CONFIRM,BUF_SIZE,0);
      }// else
    }// else if
    else if(strcmp(cmd, "pwd") == 0)
    {
      msg_to_send = "Remote working directory: " + pwd();
      send(connect_socket,msg_to_send.c_str(),BUF_SIZE,0);
    }// else if
    else if(strcmp(cmd, "quit") == 0)
    {
      // end this connection
      break;
    }// else if
    else
    {
      send(connect_socket,INVALCMD,sizeof(INVALCMD),0);
    }// else 	
  }// for
  
  live_connections.erase(connect_socket);
  close(connect_socket);
  delete this_thrd_info->socket;
  delete this_thrd_info->thrd_id;
  delete this_thrd_info->dest_info;
  delete this_thrd_info;
}// new_thread
