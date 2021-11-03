#include <iostream>
#include <unistd.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

#include "npshell.h"

using namespace std;

int main(int argc,char const *argv[]){
	int server_fd, child_socket;
	struct sockaddr_in address;
	int port = atoi(argv[1]);
	// Create socket of server
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	//reset server socket memory
	bzero((char *) &address, sizeof(address));
	//set address env
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
	//bind
	if (bind(server_fd, (struct sockaddr *)&address,sizeof(address))<0)
	{
		perror("bind failed");
		exit(EXIT_FAILURE);
	}
	//listen
	if (listen(server_fd, 0) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
	}
	setenv("PATH", "bin:.", 1);
	struct sockaddr_in child_addr;	
	int addrlen = sizeof(child_addr);
	while(1){
		child_socket = accept(server_fd,(struct sockaddr *)&child_addr,(socklen_t*)&addrlen);
			if(child_socket < 0){
				perror("accept failed");
				exit(EXIT_FAILURE);
			}
		int cpid = fork();
		while(cpid < 0){
			//fork error
			usleep(500);
			cpid = fork();
		}
		if(cpid > 0){
			//parent close socket
			close(child_socket);
		}else{
			//child exec shell
			dup2(child_socket,STDIN_FILENO);
			dup2(child_socket,STDOUT_FILENO);
			dup2(child_socket,STDERR_FILENO);
			close(child_socket);
			close(server_fd);
			Shell s;
			s.EXEC();

			exit(0);
		}
	}
	return 0;
}
