#include <iostream>
#include <stdio.h>
#include <cstdlib>
#include <stdlib.h>
#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <vector>
#include <fcntl.h>
#include <sys/stat.h>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <map>

#define CLIENTMAX 30
#define BUFSIZE 4096

using namespace std;

struct npipe{
	int in;
	int out;
	int num;
};

struct client{
	bool valid;
    int ID;
    char ip[INET6_ADDRSTRLEN];
    char nickname[20];
	int cpid;
};

int ID_arr[CLIENTMAX];
int shared_mem_fd;
int info_shared_fd;
int server_pid;
/* used to store user information */
vector<client> client_info;
/* Get  minimum number of client */
int get_min_num(){
	client *c =  (client *)mmap(NULL, sizeof(client) * CLIENTMAX, PROT_READ, MAP_SHARED, info_shared_fd, 0);
	for(int i = 0;i < CLIENTMAX;++i){
		if(!c[i].valid) return i+1;
	}
	return -1;
}

/* broadcast method with shared memory */
void broadcast(int type,string msg,int ID,int tarfd){
	char buf[BUFSIZE];
	memset( buf, 0, sizeof(char)*BUFSIZE );
	client *c =  (client *)mmap(NULL, sizeof(client) * CLIENTMAX, PROT_READ, MAP_SHARED, info_shared_fd, 0);
	switch(type){
		case 0:
			/* Login */
			sprintf(buf, "*** User '(no name)' entered from %s. ***", c[ID-1].ip);
			break;
		case 1:
			/* Change name */
			if(tarfd == -1){
				sprintf(buf,"*** User '%s' already exists. ***",msg.c_str());
				string tmpstring(buf);
				cout << tmpstring << endl;
				munmap(c, sizeof(client) * CLIENTMAX);
				return;
			}else{
				sprintf(buf,"*** User from %s is named '%s'. ***",c[ID-1].ip,msg.c_str());
			}
			break;
		case 4:
			/* Logout */
			sprintf(buf,"*** User '%s' left. ***",c[ID-1].nickname);
			break;
		default:
			perror("unknown brroadcast type");
			break;
	}
	char *p = static_cast<char*>(mmap(NULL, 0x400000, PROT_READ | PROT_WRITE, MAP_SHARED, shared_mem_fd, 0));
	string tmpstring(buf);
	/* end of string */
	tmpstring += '\0';
	strncpy(p, tmpstring.c_str(),tmpstring.length());
	munmap(p, 0x400000);
	for(int i = 0;i < CLIENTMAX;++i){
		/* check id valid */
		if(c[i].valid == true){
			kill(c[i].cpid,SIGUSR1);
		}
	}
	munmap(c, sizeof(client) * CLIENTMAX);
}

static void SIGHANDLE(int sig){
	/* receive msg from broadfcast */
	if (sig == SIGUSR1)
    {
		char *p = static_cast<char*>(mmap(NULL, 0x400000, PROT_READ, MAP_SHARED, shared_mem_fd, 0));
		string tmpstring(p);
		cout << tmpstring << endl;
		munmap(p, 0x400000);
	}
	signal(sig, SIGHANDLE);
}

class Shell
{
    private:
        //used to store numberpipe
        vector<npipe> numberpipe_vector;

        //used to store pipe
        vector<npipe> pipe_vector;
    public:
        static void HandleChild(int);
        void SETENV(string, string);
        void PRINTENV(string);
		void WHO();
		void NAME(string,int);
        int CheckBuiltIn(string *,int);
        int CheckPIPE(string,int);
        void CreatePipe(int,int,int);
        bool isWhitespace(string);
        int ParseCMD(vector<string>);
        int EXECCMD(vector<string>);
        int EXEC(int);
};

void Shell::HandleChild(int sig){
	int status;
	while(waitpid(-1,&status,WNOHANG) > 0){
	};
}

void Shell::SETENV(string name,string val){
	setenv(name.c_str(),val.c_str(),1);
}

void Shell::PRINTENV(string name){
	char *val = getenv(name.c_str());
	if(val) cout << val << endl;
}

void Shell::WHO(){
	printf("<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
	client *c =  (client *)mmap(NULL, sizeof(client) * CLIENTMAX, PROT_READ, MAP_SHARED, info_shared_fd, 0);
	for(int i = 0;i < CLIENTMAX;i++){
		if(c[i].valid == true){
			if(c[i].cpid == getpid()){
				printf("%d\t%s\t%s\t<-me\n",i+1,c[i].nickname,c[i].ip);
			}
			else
			{
				printf("%d\t%s\t%s\t\n",i+1,c[i].nickname,c[i].ip);
			}
		}
	}
	fflush(stdout);
	munmap(c, sizeof(client) * CLIENTMAX);
}

void Shell::NAME(string name,int ID){
	client *c =  (client *)mmap(NULL, sizeof(client) * CLIENTMAX, PROT_READ| PROT_WRITE, MAP_SHARED, info_shared_fd, 0);
	for(int i = 0;i < CLIENTMAX;i++){
		if(c[i].valid == true && c[i].nickname == name){
			if(c[i].cpid != getpid()){
				broadcast(1,name,ID,-1);
				return;
			}
		}
	}
	name += '\0';
	strncpy(c[ID-1].nickname,name.c_str(),name.length());
	broadcast(1,name,ID,0);
	munmap(c, sizeof(client) * CLIENTMAX);
}

int Shell::CheckBuiltIn(string *input,int ID){
	istringstream iss(*input);
	string cmd;
	getline(iss,cmd,' ');
	if(cmd == "printenv"){
		getline(iss,cmd);
		PRINTENV(cmd);
		*input = "";
		return 1;
	}else if(cmd == "setenv"){
		string name,val;
		getline(iss,name,' ');
		getline(iss,val);
		SETENV(name,val);
		*input = "";
		return 1;
	}else if(cmd == "exit"){
		*input = "";
		return -1;
	}
	else if(cmd == "who"){
		WHO();
		*input = "";
		return 1;
	}else if(cmd == "name"){
		string name;
		getline(iss,name,' ');
		NAME(name,ID);
		*input = "";
		return 1;
	}/*else if(cmd == "yell"){
		string msg;
		getline(iss,msg);
		YELL(fd,msg);
		*input = "";
		return 1;
	}else if(cmd == "tell"){
		string msg,tmp;
		getline(iss,tmp,' ');
		getline(iss,msg);
		//cerr << stoi(tmp) << endl;
		TELL(fd,msg,stoi(tmp));
		*input = "";
	}
	*/
	return 0;
}

int Shell::CheckPIPE(string input,int ID){
	vector<string> cmds;
	string delim = " | ";
	size_t pos = 0;
	bool exit_sig = false;
	while((pos = input.find(delim)) != string::npos){
		cmds.push_back(input.substr(0,pos));
		input.erase(0,pos + delim.length());
	}
	if(CheckBuiltIn(&input,ID) == -1) exit_sig = true;
	cmds.push_back(input);
	ParseCMD(cmds);
	if(exit_sig) return -1;
	return 0;
}

void Shell::CreatePipe(int in,int out,int num){
	npipe np = {in,out,num};
	pipe_vector.push_back(np);
}

bool Shell::isWhitespace(string s){
    for(int index = 0; index < s.length(); index++){
        if(!isspace(s[index]))
            return false;
    }
    return true;
}

int Shell::ParseCMD(vector<string> input){
	size_t pos = 0;
	bool has_numberpipe = false,has_errpipe = false;
	string numpipe_delim = "|";
	string errpipe_delim = "!";
	for(int i = 0;i < input.size();++i){
		string cmd;
		istringstream iss(input[i]);
		vector<string> parm;
		// Create pipe for number pipe, last one is for number
		while(getline(iss,cmd,' ')){
			if(isWhitespace(cmd)) continue;
			//if still find "!" means errorpipe with number,and record the number
			if((pos = cmd.find(errpipe_delim)) != string::npos){
				int numberpipe[2];
				int tmpnum = atoi(cmd.erase(0,pos+numpipe_delim.length()).c_str());
				for(int j = 0;j < numberpipe_vector.size();++j){
					if(tmpnum == numberpipe_vector[j].num){
						numberpipe[0] = numberpipe_vector[j].in;
						numberpipe[1] = numberpipe_vector[j].out;
					}
					else{
						pipe(numberpipe);
					}
				}
				if(numberpipe_vector.size() == 0) pipe(numberpipe);
				npipe np = {numberpipe[0],numberpipe[1],tmpnum};
				numberpipe_vector.push_back(np);
				has_errpipe = true;
				continue;
			}

			//if still find "|" means numberpipe with number,and record the number
			if((pos = cmd.find(numpipe_delim)) != string::npos){
				int numberpipe[2];
				int tmpnum = atoi(cmd.erase(0,pos+numpipe_delim.length()).c_str());
				for(int j = 0;j < numberpipe_vector.size();++j){
					if(tmpnum == numberpipe_vector[j].num){
						numberpipe[0] = numberpipe_vector[j].in;
						numberpipe[1] = numberpipe_vector[j].out;
					}
					else{
						pipe(numberpipe);
					}
				}
				if(numberpipe_vector.size() == 0) pipe(numberpipe);
				npipe np = {numberpipe[0],numberpipe[1],tmpnum};
				numberpipe_vector.push_back(np);
				has_numberpipe = true;
				continue;
			}
			parm.push_back(cmd);
		}
		if(i != input.size()-1 &&input.size() != 1){
			int pipes[2];
			pipe(pipes);
			CreatePipe(pipes[0],pipes[1],i);
		}

		signal(SIGCHLD, HandleChild);
		pid_t cpid;
		int status;
		cpid = fork();
		while (cpid < 0)
		{
			usleep(1000);
			cpid = fork();
		}
		/* Parent */
		if(cpid != 0){
			//Check fork information
			//cout << "fork " << cpid << endl;
			if(i != 0){
				close(pipe_vector[i-1].in);
				close(pipe_vector[i-1].out);
			}
			//numberpipe reciever close
			for(int j = 0;j < numberpipe_vector.size();++j){
				numberpipe_vector[j].num--;
				//numberpipe erase
				if(numberpipe_vector[j].num < 0){
					close(numberpipe_vector[j].in);
					close(numberpipe_vector[j].out);	
					numberpipe_vector.erase(numberpipe_vector.begin() + j);
					j--;
				}
			}
			if(i == input.size()-1 && !(has_numberpipe || has_errpipe)){
				waitpid(cpid,&status,0);
			}
		}
		/* Child */
		else{
			//numberpipe recieve
			if(i == 0){
				bool has_front_pipe = false;
				int front_fd = 0;
				for(int j = numberpipe_vector.size()-1;j >= 0;--j){
					if(numberpipe_vector[j].num == 0){
						if(has_front_pipe && front_fd != 0 && front_fd != numberpipe_vector[j].in){
							fcntl(front_fd, F_SETFL, O_NONBLOCK);
							while (1) {
								char tmp;
								if (read(front_fd, &tmp, 1) < 1){
									break;
								}
								int rt = write(numberpipe_vector[j].out,&tmp,1);

							}
							has_front_pipe = false;
							dup2(numberpipe_vector[j].in,STDIN_FILENO);
						}
						else{
							dup2(numberpipe_vector[j].in,STDIN_FILENO);
							front_fd = numberpipe_vector[j].in;
							has_front_pipe = true;
						}
					}
				}
				for(int j = 0;j < numberpipe_vector.size();++j)	{
					if(numberpipe_vector[j].num == 0){
						close(numberpipe_vector[j].in);
						close(numberpipe_vector[j].out);
					}
				}
			}
			//connect pipes of each child process
			if(i != input.size()-1){
				dup2(pipe_vector[i].out,STDOUT_FILENO);	
			}
			if(i != 0){
				dup2(pipe_vector[i-1].in,STDIN_FILENO);
			}
			//numberpipe send
			if(i == input.size()-1 && has_numberpipe){
				dup2(numberpipe_vector[numberpipe_vector.size()-1].out,STDOUT_FILENO);
				close(numberpipe_vector[numberpipe_vector.size()-1].in);
				close(numberpipe_vector[numberpipe_vector.size()-1].out);
			}
			if(i == input.size()-1 && has_errpipe){
				dup2(numberpipe_vector[numberpipe_vector.size()-1].out,STDERR_FILENO);
				dup2(numberpipe_vector[numberpipe_vector.size()-1].out,STDOUT_FILENO);
				close(numberpipe_vector[numberpipe_vector.size()-1].in);
				close(numberpipe_vector[numberpipe_vector.size()-1].out);
			}
			for(int j = 0;j < pipe_vector.size();j++){
				close(pipe_vector[j].in);
				close(pipe_vector[j].out);
			}
			EXECCMD(parm);
		}	
	}
	//pipe_vector.clear();
	return 0;
}

int Shell::EXECCMD(vector<string> parm){
	int fd;
	bool file_redirection = false;	
	const char **argv = new const char* [parm.size()+1];
	for(int i=0;i < parm.size();++i){
		//file redirect
		if(parm[i] == ">"){
			file_redirection = true;
			fd = open(parm.back().c_str(),O_CREAT|O_RDWR|O_TRUNC, S_IREAD|S_IWRITE);
			parm.pop_back();
			parm.pop_back();
		}
		argv[i] = parm[i].c_str();
	}
	argv[parm.size()] = NULL;
	if(file_redirection){
		// stdout to file
		if(dup2(fd,1) < 0){
			cerr << "dup error" << endl;
		}
		close(fd);
	}
	if(execvp(parm[0].c_str(),(char **)argv) == -1){
		//stderr for unknown command
		if(parm[0] != "setenv" && parm[0] != "printenv" && parm[0] != "exit")
			fprintf(stderr,"Unknown command: [%s].\n",parm[0].c_str());
		exit(0);
		//char *argv[] = {(char*)NULL};
		//execv("./bin/noop", argv);
		return -1;
	}
	return 0;
}

int Shell::EXEC(int ID){
	
	clearenv();
	SETENV("PATH","bin:.");
	while(1){
		string input;
		cout << "% ";
		getline(cin,input);
		if(!cin)
			if(cin.eof())
			{
				cout << endl;
				return 0;
			}
		if(input.empty() || isWhitespace(input)) continue;
		input.erase(remove(input.begin(), input.end(), '\n'),input.end());
		input.erase(remove(input.begin(), input.end(), '\r'),input.end());
		if(CheckPIPE(input,ID)  == -1){
			return -1;
		}
	}
	return 0;
}
