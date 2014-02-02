SOURCE=myftpclient.cpp myftpserver.cpp
OBJECT=$(SOURCE:.cpp=.o)
all:$(OBJECT)
	g++ myftpclient.o -o myftp
	g++ myftpserver.o -o myftpserver
$(OBJECT):$(SOURCE)
	g++ $(SOURCE) -c
run_server:
	./myftpserver
run_client:
	./myftp
clean:
	rm -rf *.o myftp myftpserver

