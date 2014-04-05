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
#include <signal.h>

#define BUF_SIZE 512

using namespace std;

const char CONFIRM[] = "Done";
const char INVALCMD[] = "Invalid Command";
const char ERROR[] = "Error Occurred";
const char FSEND[] = "File Send";
const char ABORT[] = "File Transfer Terminated";

socklen_t socksize = sizeof(struct sockaddr_in);

string ls();
string pwd();
int get(const int *socket, char *arg, const int);
int put(const int *socket, char *arg, const int);

void *terminating_thread(void*); // runs parallel to main and handles terminate commands
void *client_handling_thread(void*); // each new client that connects get a new thread of its own

int transfer_id = 0; // each time a get or put is received, this will be assigned to that thread and be incremented
pthread_mutex_t transfer_id_mutex = PTHREAD_MUTEX_INITIALIZER; // mutex so only 1 thread can use/increment the transfer_id variable at a time

pthread_rwlock_t rw_lock = PTHREAD_RWLOCK_INITIALIZER; // mutex for prevented simultaneously writing to a file

// to be passed in as the argument during pthread_create for client handling threads
// these are passed in due to their need to be deleted at the end of the thread
struct thrd_info
{
  int *socket;
  pthread_t *thrd_id;
  struct sockaddr_in *dest_info;
};// struct thrd_info

// keeps track of the current directory for each thread using its socket number
map<int, char*> live_connections;

// keeps track of whether the get or put command with the specified command-id is active (true) or has been terminated (false)
map<int, bool> active_transfers;


int main(int argc, char* argv[])
{
  signal(SIGPIPE, SIG_IGN);

  // check for cmd line arg
  if(argc != 3)
  {
    cout << "myftpserver requires two port numbers as command line arguments." << endl;
    exit(-1);
  }// if
  
  // retrieve port number to listen for normal commands on
  int port_num1 = atoi(argv[1]);

  // tell the user to screw off if he enters a bad port number.  atoi() error (-1) is inherently handled here.
  if(port_num1 < 1 || port_num1 > 65535)
  {
    cout << argv[1] << " is not a valid port number." << endl;
    exit(-1);
  }// if

  // retrieve port number to listen for terminate commands on
  int port_num2 = atoi(argv[2]);

  // tell the user to screw off if he enters a bad port number.  atoi() error (-1) is inherently handled here.
  if(port_num2 < 1 || port_num2 > 65535)
  {
    cout << argv[2] << " is not a valid port number." << endl;
    exit(-1);
  }// if
  
  // make sure the user didn't enter the same number twice
  if(port_num1 == port_num2)
  {
    cout << "please enter two different port numbers." << endl;
    exit(-1);
  }// else if

  // START THE TERMINATOR THREAD
  // setup socket to run on port given by second cmd ln arg
  int tcp_socket2 = socket(AF_INET, SOCK_STREAM, 0);
  if(tcp_socket2 < 0)
  {
    perror("Cannot create socket");
    exit(-1);
  }// if

  struct sockaddr_in servaddr2;
    
  servaddr2.sin_family = AF_INET;
  servaddr2.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr2.sin_port = htons(port_num2);
  
  bind(tcp_socket2,(struct sockaddr*)&servaddr2,sizeof(struct sockaddr));
  listen(tcp_socket2, 5);

  // spawn terminating thread
  pthread_t trm_thrd_id;
  struct thrd_info trm_thrd_info;

  trm_thrd_info.socket = &tcp_socket2;
  trm_thrd_info.thrd_id = &trm_thrd_id;
  pthread_create(&trm_thrd_id, NULL, terminating_thread, &trm_thrd_info);
  
  // START MAIN COMMAND HANDLING
  // get home directory
  char home_dir[BUF_SIZE];
  getcwd(home_dir, BUF_SIZE);

  int tcp_socket1 = socket(AF_INET,SOCK_STREAM,0);
  if(tcp_socket1 < 0)
  {
    perror("Cannot create socket");
    exit(-1);
  }//if
  
  struct sockaddr_in servaddr1;
  
  servaddr1.sin_family = AF_INET;
  servaddr1.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr1.sin_port = htons(port_num1);
  
  bind(tcp_socket1,(struct sockaddr*)&servaddr1,sizeof(struct sockaddr));
  listen(tcp_socket1, 5);

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

    *connect_socket = accept(tcp_socket1, (struct sockaddr*) dest, &socksize);

    live_connections[*connect_socket] = home_dir;
    pthread_create(thread_id, NULL, client_handling_thread, this_thrd_info);
  }// for

  // program should never reach this since it should stay in the above loop forever
  close(tcp_socket1);
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

int get(const int *socket, char* arg, const int transfer_id)
{
  int file_size = 0;
  char read_file[BUF_SIZE+1];
  FILE *file = fopen(arg, "r");
  if(file == NULL)
  {
    if(send(*socket,ERROR,BUF_SIZE,0) < 0)
    {      
      return -1;
    }// if
  }// if
  else
  {
    pthread_rwlock_wrlock(&rw_lock);

    // let client know file is incoming
    if(send(*socket,FSEND,BUF_SIZE,0) < 0)
    {
      pthread_rwlock_unlock(&rw_lock);
      return -1;
    }// if

    // stringify the transfer id to be sent
    char transfer_id_str[BUF_SIZE];
    sprintf(transfer_id_str, "%d", transfer_id);
    
    // give client transfer id
    if(send(*socket, transfer_id_str, BUF_SIZE, 0) < 0)
    {
      pthread_rwlock_unlock(&rw_lock);
      return -1;
    }// if

    // send file name
    if(send(*socket,arg,BUF_SIZE,0) < 0)
    {
      pthread_rwlock_unlock(&rw_lock);
      return -1;
    }// if

    // keep sending until end of file
    while(file_size = fread(read_file, sizeof(char), BUF_SIZE+1, file))
    {
      if(send(*socket,read_file,file_size,0) < 0)
      {	
        pthread_rwlock_unlock(&rw_lock);
        return -1;
      }// if
      memset(read_file,'\0',BUF_SIZE);
      // end of file reached
      if(file_size == 0)
      {
        break;
      }// if

      // make sure user hasn't terminated the transfer
      if(!active_transfers[transfer_id])
      {
        // if the user *has* terminated the transfer, then kill it, do clean up, and exit
        active_transfers.erase(transfer_id);
        fclose(file);
        pthread_rwlock_unlock(&rw_lock);
        if(send(*socket, ABORT, BUF_SIZE, 0) < 0)
        {
          pthread_rwlock_unlock(&rw_lock);
          return -1;
        }// if

        return 1;
      }// if
    }// while

    active_transfers.erase(transfer_id);
    fclose(file);
    pthread_rwlock_unlock(&rw_lock);
  }// else

  return 1;
}// get

int put(const int *socket, char *arg, const int transfer_id)
{
  int file_size = 0;
  char message[BUF_SIZE+1];
  FILE *file = fopen(basename(arg), "w");

  pthread_rwlock_wrlock(&rw_lock);

  // stringify the transfer id to be sent
  char transfer_id_str[BUF_SIZE];
  sprintf(transfer_id_str, "%d", transfer_id);
  
  // give client transfer id
  if(send(*socket, transfer_id_str, BUF_SIZE, 0) < 0)
  {
    return -1;
  }// if
  
  while(true)
  {
    // make sure user hasn't terminated the transfer
    if(!active_transfers[transfer_id])
    {
      // if the user *has* terminated the transfer, then kill it, do clean up, and exit
      active_transfers.erase(transfer_id);
      fclose(file);
      remove(arg);
      pthread_rwlock_unlock(&rw_lock);
            
      return 1;
    }// if

    file_size = recv(*socket,message,BUF_SIZE+1,0);

    if(file_size < 0)
    {
      pthread_rwlock_unlock(&rw_lock);
      return -1;
    }// if

    fwrite(message,sizeof(char),file_size,file);
    // client is done sending
    if(file_size <= BUF_SIZE)
    {
      break;
    }

  }// while

  active_transfers.erase(transfer_id);
  fclose(file);
  pthread_rwlock_unlock(&rw_lock);
  return 1;
}// put

// handle and incoming request from the client. one of these is spawned for 
void *client_handling_thread(void * thrd_info_struct)
{
  // get info on this connection
  struct thrd_info *this_thrd_info = (struct thrd_info*) thrd_info_struct;

  pthread_detach(*(this_thrd_info->thrd_id));
  
  int connect_socket = *(this_thrd_info->socket);
  
  // continue until remote user enters quit
  for(;;)
  {
    string msg_to_send = "";
    char message[BUF_SIZE];

    if(recv(connect_socket,message,BUF_SIZE,0) < 0)
    {
      break;
    }// if

    // split message into two parts
    char *cmd = strtok(message, " ");
    char *arg = strtok(NULL, " ");
    
    // switch this thread to its current directory
    chdir(live_connections[connect_socket]);
    
    if(strcmp(cmd, "get") == 0)
    {
      pthread_mutex_lock(&transfer_id_mutex);
      int t_id_assigned = transfer_id++; // get transfer_id for this command and increment it
      pthread_mutex_unlock(&transfer_id_mutex);

      active_transfers[t_id_assigned] = true;
      if(get(&connect_socket,arg, t_id_assigned) < 0)
      {
        break;
      }// if
    } // if
    else if(strcmp(cmd, "put") == 0)
    {
      pthread_mutex_lock(&transfer_id_mutex);
      int t_id_assigned = transfer_id++; // get transfer_id for this command and increment it
      pthread_mutex_unlock(&transfer_id_mutex);
      
      active_transfers[t_id_assigned] = true;
      if(put(&connect_socket,arg, t_id_assigned) < 0)
      {
        break;
      }// if
    }// else if
    else if(strcmp(cmd, "delete") == 0)
    {
      if(remove(arg) != 0)
      {
        // send error message upon fail
        if(send(connect_socket,ERROR,BUF_SIZE,0) < 0)
        {
          break;
        }// if
      }// if
      else
      {  
      	msg_to_send = "Removing ";
      	if(send(connect_socket,msg_to_send.append(arg).c_str(),BUF_SIZE,0) < 0)
      	{
      	  break;
      	}// if
      }// else
    }// else if
    else if(strcmp(cmd, "ls") == 0) 
    {
      msg_to_send = ls();
      if(send(connect_socket,msg_to_send.c_str(),BUF_SIZE,0) < 0)
      {
        break;
      }// if
    }// else if
    else if(strcmp(cmd, "cd") == 0)
    {
      if(chdir(arg) != 0)
      {
        // send error message upon fail
        if(send(connect_socket,ERROR,BUF_SIZE,0) < 0)
        {
          break;
        }// if
      }// if
      else
      { 
        if(send(connect_socket,CONFIRM,BUF_SIZE,0) < 0)
        {
          break;
        }// if 

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
      	if(send(connect_socket,ERROR,BUF_SIZE,0) < 0)
      	{
      	  break;
      	}// if
      }// if
      else
      {
      	if(send(connect_socket,CONFIRM,BUF_SIZE,0) < 0)
      	{
      	  break;
      	}// if 
      }// else
    }// else if
    else if(strcmp(cmd, "pwd") == 0)
    {
      msg_to_send = "Remote working directory: " + pwd();
      if(send(connect_socket,msg_to_send.c_str(),BUF_SIZE,0) < 0)
      {
        break;
      }// if
    }// else if
    else if(strcmp(cmd, "quit") == 0)
    {
      // end this connection
      break;
    }// else if
    else
    {
      if(send(connect_socket,INVALCMD,sizeof(INVALCMD),0) < 0)
      {
        break;
      }// if
    }// else 	
  }// for
  
  live_connections.erase(connect_socket);
  close(connect_socket);
  delete this_thrd_info->socket;
  delete this_thrd_info->thrd_id;
  delete this_thrd_info->dest_info;
  delete this_thrd_info;

  pthread_exit(NULL);
}// client_handling_thread

void *terminating_thread(void *thrd_info_struct)
{
  // get info on this connection
  struct thrd_info *this_thrd_info = (struct thrd_info*) thrd_info_struct;
  int tcp_sock = *(this_thrd_info->socket);

  pthread_detach(*(this_thrd_info->thrd_id));

  // run until program ends
  for(;;)
  {
    struct sockaddr_in *dest = new struct sockaddr_in;
    char message[BUF_SIZE]; 

    int connect_socket = accept(tcp_sock, (struct sockaddr*) dest, &socksize);
    recv(connect_socket, message, BUF_SIZE, 0);

    // convert message to int. bad messages returns -1
    int transfer_id = atoi(message);
    
    // get state of given transfer.  non-existant transfer will put -1 in transfer_id
    map<int, bool>::iterator at_iter = active_transfers.find(transfer_id);
    if(at_iter == active_transfers.end())
    {
      transfer_id = -1;
    }// if

    // if transfer_id == -1 -> notify user of his failure at life and move to next request
    if(transfer_id == -1)
    {
      send(connect_socket, INVALCMD, BUF_SIZE, 0);
      continue;
    }// if

    // if given transfer_id is found, set it to false
    active_transfers[transfer_id] = false;
    send(connect_socket, CONFIRM, sizeof(CONFIRM), 0);
  }// for
}// terminating_thread
