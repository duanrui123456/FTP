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

/*
	these are the function we need to implement
	get
	put
	delete
	ls
	cd
	mkdir
	pwd
	quit

	threads with pthreads package
*/
int ls();

int main(int argc, char* argv[])
{
	int tcp_socket = socket(AF_INET,SOCK_STREAM,0);
	if(tcp_socket < 0)
	{
		perror("cannot create socket");
	}

	struct sockaddr_in dest;
	struct sockaddr_in servaddr;
	socklen_t socksize = sizeof(struct sockaddr_in);

	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port = htons(PORTNUM);

	bind(tcp_socket,(struct sockaddr* )&servaddr,sizeof(struct sockaddr));
	listen(tcp_socket,5);
	int connect_socket = accept(tcp_socket,(struct sockaddr*)&dest,&socksize);

	string confirm = "Message Received";
	char message[512];

	while(connect_socket)
	{
		printf("Incoming Transmission from %s\n",inet_ntoa(dest.sin_addr));
		recv(connect_socket,message,512,0);
		string str = message;
		if(str == "ls") // check received commands
		{
				ls();
		}

		cout << "Message Received: " << message << endl;
		send(connect_socket,confirm.c_str(),512,0);
		connect_socket = accept(tcp_socket,(struct sockaddr*)&dest,&socksize); // wait for another connection
	}
	close(tcp_socket);
	close(connect_socket);
	return EXIT_SUCCESS;
}

int ls()
{
	DIR *directory;
	struct dirent *reader;
	directory = opendir("."); // open current directory
	if(directory == NULL)
	{	
		perror("Cannot open directory");
		return -1;
	}
	reader = readdir(directory);
	while(reader != NULL)
	{
		cout << reader->d_name << " ";

	}
	closedir(directory);
	return 0;
}