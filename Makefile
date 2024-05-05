

all: ftp_server


ftp_server: src/ClientConnection/ClientConnection.cpp src/Server/FTPServer.cpp src/ftp_server.cpp
	g++ -g -std=gnu++0x  src/ClientConnection/ClientConnection.cpp src/Server/FTPServer.cpp src/ftp_server.cpp -o bin/ftp_server -lpthread

clean:
	$(RM) ftp_server bin/*
