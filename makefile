all:myftpclient.o myftpserver.o
	g++ -pthread myftpclient.o -o myftp
	g++ -pthread myftpserver.o -o myftpserver
myftpclient.o:myftpclient.cpp
	g++ -pthread myftpclient.cpp -c
myftpserver.o:myftpserver.cpp
	g++ -pthread myftpserver.cpp -c
clean:
	rm -rf *.o myftp myftpserver