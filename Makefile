

all: ftp_server


ftp_server: src/ClientConnection.cpp src/FTPServer.cpp src/ftp_server.cpp
	g++ -g -std=gnu++0x  src/ClientConnection.cpp src/FTPServer.cpp src/ftp_server.cpp -o bin/ftp_server -lpthread

clean:
	$(RM) ftp_server *~
