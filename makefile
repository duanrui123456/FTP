all:myftpclient.o myftpserver.o
	g++ myftpclient.o -o myftp
	g++ -pthread myftpserver.o -o myftpserver
myftpclient.o:myftpclient.cpp
	g++ myftpclient.cpp -c
myftpserver.o:myftpserver.cpp
	g++ -pthread myftpserver.cpp -c
client:
	./myftp 127.0.0.1 3019
server:
	./myftpserver 3019 &
clean:
	rm *.o myftp myftpserver
