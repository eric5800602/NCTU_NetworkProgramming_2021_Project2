
#include "np_multi_proc.h"

using namespace std;

bool sortid(client s1, client s2){
   return s1.ID < s2.ID;
}

void welcome(int fd)
{
    string buf =
        "****************************************\n\
** Welcome to the information server. **\n\
****************************************";
    cout << buf << endl;
    return;
}
void ServerSigHandler(int sig)
{
	if (sig == SIGCHLD)
    {
		while(waitpid (-1, NULL, WNOHANG) > 0);
	}
    else if(sig == SIGINT || sig == SIGQUIT || sig == SIGTERM)
    {
		exit (0);
	}
	signal (sig, ServerSigHandler);
}

void init_info_shared_memory(){
	/* shared memory store client info */
	info_shared_fd = shm_open("used_to_store_client_info", O_CREAT | O_RDWR, 0666);
	ftruncate(info_shared_fd, sizeof(client) * CLIENTMAX);
	client *c =  (client *)mmap(NULL, sizeof(client) * CLIENTMAX, PROT_READ | PROT_WRITE, MAP_SHARED, info_shared_fd, 0);
	for(int i = 0;i < CLIENTMAX;++i){
		c[i].valid = false;
	}
	munmap(c, sizeof(client) * CLIENTMAX);
}

void init_FIFO_shared_memory(){
	/* shared memory store client info */
	userpipe_shared = shm_open("used_to_store_userpipe", O_CREAT | O_RDWR, 0666);
	ftruncate(userpipe_shared, sizeof(fifo_info));
	fifo_info* f =  (fifo_info *)mmap(NULL, sizeof(fifo_info) , PROT_READ | PROT_WRITE, MAP_SHARED, userpipe_shared, 0);
	for(int i = 0;i < CLIENTMAX;++i){
		for(int j = 0;j < CLIENTMAX;++j){
			f->fifolist[i][j].used = false;
			f->fifolist[i][j].in = -1;
			f->fifolist[i][j].out = -1;
			memset(&f->fifolist[i][j].name,0,sizeof(f->fifolist[i][j].name));
		}
	}
	munmap(f,  sizeof(fifo_info));
}

int main(int argc,char const *argv[]){
	/* Initiallize user pipe folder */
	if(NULL==opendir(PIPE_PATH))
   		mkdir(PIPE_PATH,0777);
	/* Open shared memory */
	server_pid = getpid();
	shared_mem_fd = shm_open("used_to_broadcast", O_CREAT | O_RDWR, 0666);
	ftruncate(shared_mem_fd, 0x400000);
	init_info_shared_memory();
	init_FIFO_shared_memory();
	/* socket setting */
	int server_fd, child_socket;
	struct sockaddr_in address;
	int port = atoi(argv[1]);
	// Create socket of server
	if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) <= 0)
	{
		perror("socket failed");
		exit(EXIT_FAILURE);
	}
	int optval = 1;
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int)) == -1) 
    {
		perror("Error: set socket failed");
		return 0;
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
	while(1){
		struct sockaddr_in child_addr;	
		int addrlen = sizeof(child_addr);
		child_socket = accept(server_fd,(struct sockaddr *)&child_addr,(socklen_t*)&addrlen);
		
		/* Check number of clients*/
		int ID = get_min_num();
		if(ID < 0){
			perror("Client MAX");
			return 0;
		}
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
			signal (SIGCHLD, ServerSigHandler);
			signal (SIGINT, ServerSigHandler);
			signal (SIGQUIT, ServerSigHandler);
			signal (SIGTERM, ServerSigHandler);
			close(child_socket);
		}else{
            /* client ip */
			dup2(child_socket,STDIN_FILENO);
			dup2(child_socket,STDOUT_FILENO);
			dup2(child_socket,STDERR_FILENO);
			close(child_socket);
			close(server_fd);
			/* set info shared memory */
			void *p = mmap(NULL, sizeof(client) * CLIENTMAX, PROT_READ | PROT_WRITE, MAP_SHARED, info_shared_fd, 0);
			client *cf = (client *) p;
			char ip[INET6_ADDRSTRLEN];
            sprintf(ip, "%s:%d", inet_ntoa(child_addr.sin_addr), ntohs(child_addr.sin_port));
			strncpy(cf[ID-1].ip,ip,INET6_ADDRSTRLEN);
			strcpy(cf[ID-1].nickname,"(no name)");
			cf[ID-1].cpid = getpid();
			cf[ID-1].valid = true;
			munmap(p, sizeof(client) * CLIENTMAX);
			/* Set signals for boradcast msg */
			signal(SIGUSR1, SIGHANDLE);
			signal(SIGUSR2, SIGHANDLE);
			/* Set signals for client */
			signal(SIGINT, SIGHANDLE);
			signal(SIGQUIT, SIGHANDLE);
			signal(SIGTERM, SIGHANDLE);
			/* send welcome msg */
			welcome(child_socket);
			/* broadcast login information */
			broadcast(0, "", ID, 0);
			Shell s;
			//child exec shell
			if(s.EXEC(ID) == -1){
				close(STDIN_FILENO);
                close(STDOUT_FILENO);
                close(STDERR_FILENO);
				client *c =  (client *)mmap(NULL, sizeof(client) * CLIENTMAX, PROT_READ | PROT_WRITE, MAP_SHARED, info_shared_fd, 0);
				c[ID-1].valid = false;
				broadcast(4,"",ID,0);
				EraseUserPipe(ID);
				munmap(c, sizeof(client) * CLIENTMAX);
                exit(0);
			}
		}
	}
	return 0;
}
