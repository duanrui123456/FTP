#include <sys/socket.h>
#include <arpa/inet.h>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <iostream>

#define MAXADDRLEN 256
#define BUFFER 128
#define PORTNUM 3019
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
	
	string info = "";
	cout << "Enter any string. Enter '0' to quit.\n";
	cin >> info;
	char message[512];

	send(tcp_socket,info.c_str(),512,0);
	recv(tcp_socket,message,512,0);
	cout << message << endl;
	close(tcp_socket);
}
