#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <cstdlib>
#include <dirent.h>

#define PORTNUM 3019
using namespace std;

/*	these are the function we need to implement
	get
	put
	delete
	ls
	cd
	mkdir
	pwd
	quit

	use threads with pthreads package
*/
void ls();

int main(int argc, char* argv[])
{
	int tcp_socket = socket(AF_INET,SOCK_STREAM,0);
	if(tcp_socket < 0)
		perror("cannot create socket");

	struct sockaddr_in dest;
	struct sockaddr_in servaddr;
	socklen_t socksize = sizeof(struct sockaddr_in);

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORTNUM);

	bind(tcp_socket,(struct sockaddr* )&servaddr,sizeof(struct sockaddr));
	listen(tcp_socket,5);
	int connect_socket = accept(tcp_socket,(struct sockaddr*)&dest,&socksize);

	const char CONFIRM[] = "Message Received";
	const char INVALCMD[] = "Invalid Command";
	char message[512];

	while(connect_socket)
	{
		printf("Incoming Transmission from %s\n",inet_ntoa(dest.sin_addr));
		recv(connect_socket,message,512,0);
		//cout << "Message Received: " << message << endl;
		
		/* check received commands */
		const string str = message; 

		//if(str == "get"){} 
		//else if(str == "put")
		//else if(str == "delete")
		if(str == "ls") ls();
		//else if(str == "cd")
		//else if(str == "mkdir")
		//else if(str == "pwd")
		//else if(str == "quit")
		//else
			//send(connect_socket,INVALCMD,sizeof(INVALCMD),0);

		//send(connect_socket,CONFIRM,512,0);
		
		/* wait for another connection */
		connect_socket = accept(tcp_socket,(struct sockaddr*)&dest,&socksize); 	}
	close(tcp_socket);
	close(connect_socket);
	return EXIT_SUCCESS;
}

void ls()
{
	DIR *directory;
	struct dirent *reader;
	/* open current directory */
	directory = opendir("."); 
	if(directory == NULL)
		perror("Cannot open directory");

	while((reader = readdir(directory))!= NULL)
	{	
		printf("%s ",reader->d_name);
	}
	closedir(directory);
}