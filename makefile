all:client.o server.o
	g++ client.o -o client
	g++ server.o -o server
client.o:client.cpp
	g++ client.cpp -c
server.o:server.cpp
	g++ server.cpp -c
run_server:
	./server
run_client:
	./client
clean:
	rm -rf client server *.o

