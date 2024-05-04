//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//              This class processes an FTP transaction.
// 
//****************************************************************************


#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h> 
#include <iostream>
#include <dirent.h>

#include "common.h"

#include "ClientConnection.h"
#include "FTPServer.h"




ClientConnection::ClientConnection(int s) {
    int sock = (int)(s);
  
    char buffer[MAX_BUFF];

    control_socket = s;
    // Check the Linux man pages to know what fdopen does.
    fd = fdopen(s, "a+");
    if (fd == NULL){
	std::cout << "Connection closed" << std::endl;

	fclose(fd);
	close(control_socket);
	ok = false;
	return ;
    }
    
    ok = true;
    data_socket = -1;
    parar = false;
   
  
  
};


ClientConnection::~ClientConnection() {
 	fclose(fd);
	close(control_socket); 
  
}


int connect_TCP( uint32_t address,  uint16_t  port) {
	// Implement your code to define a socket here
	struct sockaddr_in sin;
	int fd;
	// SOCK_STREAM used for TCP, SOCK_DGRAM used for UDP
	fd = socket(AF_INET, SOCK_STREAM, 0);

	if( fd < 0 ) {
		errexit("ERROR %s: Can't create the socket", strerror(errno));
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = address;
	sin.sin_port = htons(port);

	if(connect( fd, (struct sockaddr * )& sin, sizeof(sin)) < 0) {
		errexit("ERROR %s: Can't connect with socket", strerror(errno));
	}
	
	return fd; // You must return the socket descriptor.
}

void ClientConnection::stop() {
    close(data_socket);
    close(control_socket);
    parar = true;
  
}
    
#define COMMAND(cmd) strcmp(command, cmd)==0

// This method processes the requests.
// Here you should implement the actions related to the FTP commands.
// See the example for the USER command.
// If you think that you have to add other commands feel free to do so. You 
// are allowed to add auxiliary methods if necessary.

void ClientConnection::WaitForRequests() {
	if (!ok) {
		return;
	}

	bool Logged = false;
	bool userInputted = false;
	fprintf(fd, "220 Service ready\n");
	
	while(!parar) {

		fscanf(fd, "%s", command);
		if (COMMAND("USER")) {
			if (userInputted) {
				Logged = false;
				userInputted = false;
				fflush(fd);
			}
			fscanf(fd, "%s", arg);
			fprintf(fd, "331 User name ok, need password\n");
			userInputted = true;
		}
		else if (COMMAND("PASS")) {
			if (!userInputted) {
				fprintf(fd, "530 Not logged in\n");
				parar = true;
			}
			fscanf(fd, "%s", arg);
			if (strcmp(arg,"1234") == 0) {
					fprintf(fd, "230 User logged in\n");
					Logged = true;
			}
			else {
					fprintf(fd, "530 Not logged in\n");
					parar = true;
			}
		}
		else if (Logged) {
			if (COMMAND("PWD")) {
				fprintf(fd, "257 \"%s\" created", get_current_dir_name());
			}
			// PORT h1,h2,h3,h4,p1,p2
			else if (COMMAND("PORT")) {
				int h1, h2, h3, h4, p1, p2;
				fscanf(fd, "%d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2);
				uint32_t address = h4 << 24 || h3 << 16 || h2 << 8 || h1;
				uint16_t port = connect_TCP(address, port);
				fprintf(fd, "200 OK\n");
			}
			else if (COMMAND("PASV")) {
				int newSocket, p1, p2;
				struct sockaddr_in sin;
				socklen_t slen = sizeof(sin);
				// As it needs to listen, let's use define_socket_TCP() which includes a listen()
				// from FTPServer
				newSocket = define_socket_TCP(0);
				getsockname(newSocket, reinterpret_cast<sockaddr*>(&sin), &slen);
				uint16_t port = sin.sin_port;
				p1 = port >> 8;
				p2 = port && 0xFF;
				fprintf(fd, "227 Entering Passive Mode (127,0,0,1,%d,%d)", p1, p2);
				fflush(fd);
				data_socket = accept(newSocket, reinterpret_cast<sockaddr*>(&sin), &slen);
			}
			else if (COMMAND("STOR") ) {
				fscanf(fd, "%s", arg);
				FILE *fp;
				if (fp == NULL) {
					fprintf(fd, "553 Requested action not taken. File name not allowed.\n");
					continue;
				}
				fprintf(fd, "150 File status okay; about to open data connection\n");
				fflush(fd);
				int bufSize = 2048;
				char buffer[bufSize];
				int b = 2048;
				while (b == 2048) {
					int b = recv(data_socket, buffer, bufSize, 0);
					fwrite(buffer, 1, b, fp);
				}

				fprintf(fd, "226 Closing data connection\n");
				fflush(fd);
				close(data_socket);
				fclose(fp);
			}
			else if (COMMAND("RETR")) {
				fscanf(fd, "%s", arg);
				FILE *fp;
				fp = fopen(arg, "rb");
				if (fp == NULL){
					fprintf(fd, "553 Requested action not taken. File not allowed\n");
					continue;
				}
				fprintf(fd, "150 File status okay; about to open data connection\n");
				fflush(fd);
				int bufSize = 2048;
				char buffer[bufSize];
				int b = 2048;
				while (b == 2048) {
					int b = fread(buffer, 1, bufSize, fp);
					send(data_socket, buffer, b, 0);
				}

				fprintf(fd, "226 Closing data connection\n");
				fflush(fd);
				close(data_socket);
				fclose(fp);


			}
			else if (COMMAND("LIST")) {
				DIR* dp;
				dp = opendir(get_current_dir_name());
				struct dirent* dirp;
				fprintf(fd, "125 List started OK\n");
				while((dirp = readdir(dp)) != NULL) {
					std::string content = std::string(dirp -> d_name) + "\n";
					send(data_socket, content.c_str(), content.size(), 0);
				}
				closedir(dp);
				fprintf(fd, "250 List completed successfully");
			}
			else if (COMMAND("SYST")) {
				fprintf(fd, "215 UNIX Type: L8.\n");   
			}

			else if (COMMAND("TYPE")) {
				fscanf(fd, "%s", arg);
				fprintf(fd, "200 OK\n");   
			}
			
			else if (COMMAND("QUIT")) {
				fprintf(fd, "221 Service closing control connection. Logged out if appropriate.\n");
				close(data_socket);	
				parar=true;
				break;
			}

			else  {
				fprintf(fd, "502 Command not implemented.\n"); fflush(fd);
				printf("Comando : %s %s\n", command, arg);
				printf("Error interno del servidor\n");
			}
		}
		else if (COMMAND("PWD")  || COMMAND("PORT") || COMMAND("PASV") ||
						 COMMAND("STOR") || COMMAND("RETR") || COMMAND("LIST") ||
						 COMMAND("SYST") || COMMAND("TYPE") || COMMAND("QUIT")) {
			fprintf(fd, "530 Not logged in.\n");
			parar = true;
		}
		else {
			fprintf(fd, "502 Command not implemented.\n"); fflush(fd);
			printf("Comando : %s %s\n", command, arg);
			printf("Error interno del servidor\n");
		}
		
	}
	
	fclose(fd);

	
	return;

};
