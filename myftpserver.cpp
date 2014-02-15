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

#define BUF_SIZE 512

using namespace std;

/*	these are the function we need to implement
	get----------done
	put----------done
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

  // tell the user to screw off if he enters a bad port number
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
  
  struct sockaddr_in dest;
  struct sockaddr_in servaddr;
  socklen_t socksize = sizeof(struct sockaddr_in);
  
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(port_num);
  
  bind(tcp_socket,(struct sockaddr* )&servaddr,sizeof(struct sockaddr));
  listen(tcp_socket,5);
  int connect_socket = 0;
    
  // accept connections
  while(connect_socket = accept(tcp_socket,(struct sockaddr*)&dest,&socksize))
  {
    // printf("Incoming Transmission from %s\n",inet_ntoa(dest.sin_addr));

    // continue until remote user enters quit
    for(;;)
    {
      string msg_to_send = "";
      char message[BUF_SIZE];
      recv(connect_socket,message,BUF_SIZE,0);
      
      // split message into two parts
      char *cmd = strtok(message, " ");
      char *arg = strtok(NULL, " ");

      if(strcmp(cmd, "get") == 0)
      {
        get(&connect_socket,arg);
      } // if
      else if(strcmp(cmd, "put") == 0)
      {
        put(&connect_socket,arg);
        send(connect_socket,CONFIRM,BUF_SIZE,0);
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
        }
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
      	close(connect_socket);
	      chdir(home_dir);
      	break;
      }// else if
      else
      {
        send(connect_socket,INVALCMD,sizeof(INVALCMD),0);
      }// else 	
    }// for
  }// while
  close(tcp_socket);
  
  return EXIT_SUCCESS;
}// main

string ls()
{
  DIR *directory;
  struct dirent *reader;
  // open current directory
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
  char read_file[BUF_SIZE];
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
    while(file_size = fread(read_file, sizeof(char), BUF_SIZE, file))
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
  char message[BUF_SIZE];
  FILE *file = fopen(basename(arg), "w");
  // keep receiving file until it CONFIRM is sent
  while(file_size = recv(*socket,message,BUF_SIZE,0))
  {
    fwrite(message,sizeof(char),file_size,file);
    // client is done sending
    if(file_size < BUF_SIZE)
    {
      break;
    }
  }// while
  fclose(file);
}// put