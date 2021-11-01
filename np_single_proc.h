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
#include <map>
#define QLEN 5
#define BUFSIZE 4096
#define CLIENTMAX 30

using namespace std;

struct npipe{
	int in;
	int out;
	int num;
};

struct client{
    int ID;
    string ip;
    string nickname;
	int fd;
	/* used to store numberpipe */
	vector<npipe> numberpipe_vector;
	/* used to sotre user env setting */
	map<string, string> mapenv;
};

/* used to store user information */
vector<client> client_info;
int msock;               /* master server socket	*/
fd_set rfds;             /* read file descriptor set	*/
fd_set afds;             /* active file descriptor set */
int ID_arr[CLIENTMAX];

int get_min_num(){
	for(int i = 0;i < CLIENTMAX;++i){
		if(ID_arr[i] == 0){
			ID_arr[i] = 1;
			return i+1;
		}
	}
	return -1;
}

void broadcast(int type,string msg,client *cnt,int tarfd){
	int nfds = getdtablesize();
	char buf[BUFSIZE];
    bzero((char *)&buf, BUFSIZE);
	client c = *cnt;
	switch(type){
		case 0:
			/* Login */
			sprintf(buf, "*** User ’(no name)’ entered from %s. ***\n", c.ip.c_str());
			break;
		case 1:
			/* Change name */
			sprintf(buf,"*** User from %s is named ’%s’. ***\n",c.ip.c_str(),msg.c_str());
			break;
		case 2:
			/* Yell with msg */
			sprintf(buf,"*** %s yelled ***: %s\n",c.nickname.c_str(),msg.c_str());
			break;
		case 3:
			/* Tell with msg and target */
			if(tarfd == -1){
				sprintf(buf,"*** Error: user #%s does not exist yet. ***\n",msg.c_str());
				if(send(c.fd, buf, BUFSIZE,0) < 0)
					perror("Tell unknown error");
			}else{
				sprintf(buf,"*** %s told you ***: %s\n",c.nickname.c_str(),msg.c_str());
				if(send(tarfd, buf, BUFSIZE,0) < 0)
					perror("Tell error");
			}
			break;
		case 4:
			/* Logout */
			sprintf(buf,"*** User ’%s’ left. ***\n",c.nickname.c_str());
			break;
		default:
			perror("unknown brroadcast type");
			break;
	}
	/* Not tell msg */
	if(type != 3)
		for (int fd = 0; fd < nfds; ++fd)
		{
			//send to all active fd
			if (fd != msock && FD_ISSET(fd, &afds))
			{
				int cc = send(fd, buf, BUFSIZE,0);
			}
		}
}

void DeleteClient(int fd){
	for(int i = 0;i < client_info.size();i++){
		client c = client_info[i];
		if(c.fd == fd){
			client_info.erase(client_info.begin()+i);
			ID_arr[c.ID-1] = 0;
		}
	}
}

class Shell
{
    private:
        //used to store pipe
        vector<npipe> pipe_vector;
    public:
        static void HandleChild(int);
        void SETENV(string, string,int);
        string PRINTENV(string,int);
		void WHO(int);
		void NAME(int,string);
		void YELL(int,string);
		void TELL(int,string,int);
        int CheckBuiltIn(string*,int);
        int CheckPIPE(string,int);
        void CreatePipe(int,int,int);
        bool isWhitespace(string);
        int ParseCMD(vector<string>,int);
        int EXECCMD(vector<string>,int);
        int EXEC(string,int);
};

void Shell::HandleChild(int sig){
	int status;
	while(waitpid(-1,&status,WNOHANG) > 0){
	};
}

void Shell::SETENV(string name,string val,int fd){
	client *c;
	for(int i = 0;i < client_info.size();i++){
		c = &client_info[i];
		if(c->fd == fd){
			c->mapenv[name] = val;
			break;
		}
	}
	//setenv(name.c_str(),val.c_str(),1);
}

string Shell::PRINTENV(string name,int fd){
	client *c;
	for(int i = 0;i < client_info.size();i++){
		c = &client_info[i];
		if(c->fd == fd){
			break;
		}
	}
	auto it = c->mapenv.find(name); 
	if (it != c->mapenv.end()) {
		return it->second;
    }
	return  name + " not found!";
}

void Shell::WHO(int fd){
	printf("<ID>\t<nickname>\t<IP:port>\t<indicate me>\n");
	for(int i = 0;i < client_info.size();i++){
		client c = client_info[i];
		if(c.fd == fd){
			printf("%d\t%s\t%s\t<-me\n",c.ID,c.nickname.c_str(),c.ip.c_str());
		}
		else{
			printf("%d\t%s\t%s\t\n",c.ID,c.nickname.c_str(),c.ip.c_str());
		}
	}
	fflush(stdout);
}

void Shell::NAME(int fd,string name){
	for(int i = 0;i < client_info.size();i++){
		client *c = &client_info[i];
		if(c->fd == fd){
			c->nickname = name;
			broadcast(1,name,c,0);
		}
	}
}

void Shell::YELL(int fd,string msg){
	for(int i = 0;i < client_info.size();i++){
		client *c = &client_info[i];
		if(c->fd == fd){
			broadcast(2,msg,c,0);
		}
	}
}

void Shell::TELL(int fd,string msg,int target){
	client *me;
	int tarfd = -1;
	for(int i = 0;i < client_info.size();i++){
		client *c = &client_info[i];
		if(c->fd == fd){
			me = c;
		}
		if(c->ID == target){
			tarfd = c->fd;
		}
	}
	if(tarfd == -1) msg = to_string(target);
	broadcast(3,msg,me,tarfd);
}

int Shell::CheckBuiltIn(string *input,int fd){
	istringstream iss(*input);
	string cmd;
	getline(iss,cmd,' ');
	if(cmd == "printenv"){
		getline(iss,cmd,' ');
		cout << PRINTENV(cmd,fd) << endl;
		*input = "";
		return 1;
	}else if(cmd == "setenv"){
		string name,val;
		getline(iss,name,' ');
		getline(iss,val,' ');
		SETENV(name,val,fd);
		*input = "";
		return 1;
	}else if(cmd == "exit"){
		*input = "";
		return -1;
	}else if(cmd == "who"){
		WHO(fd);
		*input = "";
		return 1;
	}else if(cmd == "name"){
		string name;
		getline(iss,name,' ');
		NAME(fd,name);
		*input = "";
		return 1;
	}else if(cmd == "yell"){
		string msg;
		getline(iss,msg,' ');
		YELL(fd,msg);
		*input = "";
		return 1;
	}else if(cmd == "tell"){
		string msg,tmp;
		getline(iss,tmp,' ');
		getline(iss,msg,' ');
		//cerr << stoi(tmp) << endl;
		TELL(fd,msg,stoi(tmp));
		*input = "";
	}
	return 0;
}

int Shell::CheckPIPE(string input,int fd){
	vector<string> cmds;
	string delim = " | ";
	size_t pos = 0;
	bool exit_sig = false;
	while((pos = input.find(delim)) != string::npos){
		cmds.push_back(input.substr(0,pos));
		input.erase(0,pos + delim.length());
	}
	if(CheckBuiltIn(&input,fd) == -1) exit_sig = true;
	cmds.push_back(input);
	ParseCMD(cmds,fd);
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

int Shell::ParseCMD(vector<string> input,int fd){
	size_t pos = 0;
	bool has_numberpipe = false,has_errpipe = false;
	client *c;
	for(int i = 0;i < client_info.size();i++){
		c = &client_info[i];
		if(c->fd == fd) break;
	}
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
				for(int j = 0;j < c->numberpipe_vector.size();++j){
					if(tmpnum == c->numberpipe_vector[j].num){
						numberpipe[0] = c->numberpipe_vector[j].in;
						numberpipe[1] = c->numberpipe_vector[j].out;
					}
					else{
						pipe(numberpipe);
					}
				}
				if(c->numberpipe_vector.size() == 0) pipe(numberpipe);
				npipe np = {numberpipe[0],numberpipe[1],tmpnum};
				c->numberpipe_vector.push_back(np);
				has_errpipe = true;
				continue;
			}

			//if still find "|" means numberpipe with number,and record the number
			if((pos = cmd.find(numpipe_delim)) != string::npos){
				int numberpipe[2];
				int tmpnum = atoi(cmd.erase(0,pos+numpipe_delim.length()).c_str());
				for(int j = 0;j < c->numberpipe_vector.size();++j){
					if(tmpnum == c->numberpipe_vector[j].num){
						numberpipe[0] = c->numberpipe_vector[j].in;
						numberpipe[1] = c->numberpipe_vector[j].out;
					}
					else{
						pipe(numberpipe);
					}
				}
				if(c->numberpipe_vector.size() == 0) pipe(numberpipe);
				npipe np = {numberpipe[0],numberpipe[1],tmpnum};
				c->numberpipe_vector.push_back(np);
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
			for(int j = 0;j < c->numberpipe_vector.size();++j){
				c->numberpipe_vector[j].num--;
				//numberpipe erase
				if(c->numberpipe_vector[j].num < 0){
					close(c->numberpipe_vector[j].in);
					close(c->numberpipe_vector[j].out);	
					c->numberpipe_vector.erase(c->numberpipe_vector.begin() + j);
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
				for(int j = c->numberpipe_vector.size()-1;j >= 0;--j){
					if(c->numberpipe_vector[j].num == 0){
						if(has_front_pipe && front_fd != 0 && front_fd != c->numberpipe_vector[j].in){
							fcntl(front_fd, F_SETFL, O_NONBLOCK);
							while (1) {
								char tmp;
								if (read(front_fd, &tmp, 1) < 1){
									break;
								}
								int rt = write(c->numberpipe_vector[j].out,&tmp,1);

							}
							has_front_pipe = false;
							dup2(c->numberpipe_vector[j].in,STDIN_FILENO);
						}
						else{
							dup2(c->numberpipe_vector[j].in,STDIN_FILENO);
							front_fd = c->numberpipe_vector[j].in;
							has_front_pipe = true;
						}
					}
				}
				for(int j = 0;j < c->numberpipe_vector.size();++j)	{
					if(c->numberpipe_vector[j].num == 0){
						close(c->numberpipe_vector[j].in);
						close(c->numberpipe_vector[j].out);
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
				dup2(c->numberpipe_vector[c->numberpipe_vector.size()-1].out,STDOUT_FILENO);
				close(c->numberpipe_vector[c->numberpipe_vector.size()-1].in);
				close(c->numberpipe_vector[c->numberpipe_vector.size()-1].out);
			}
			if(i == input.size()-1 && has_errpipe){
				dup2(c->numberpipe_vector[c->numberpipe_vector.size()-1].out,STDERR_FILENO);
				dup2(c->numberpipe_vector[c->numberpipe_vector.size()-1].out,STDOUT_FILENO);
				close(c->numberpipe_vector[c->numberpipe_vector.size()-1].in);
				close(c->numberpipe_vector[c->numberpipe_vector.size()-1].out);
			}
			for(int j = 0;j < pipe_vector.size();j++){
				close(pipe_vector[j].in);
				close(pipe_vector[j].out);
			}
			EXECCMD(parm,fd);
		}	
	}
	//pipe_vector.clear();
	return 0;
}

int Shell::EXECCMD(vector<string> parm,int client_fd){
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
	clearenv();
	setenv("PATH",PRINTENV("PATH",client_fd).c_str(),1);
	if(execvp(parm[0].c_str(),(char **)argv) == -1){
		//stderr for unknown command
		if(parm[0] != "setenv" && parm[0] != "printenv" && parm[0] != "exit" &&
		   parm[0] != "who" && parm[0] != "name" && parm[0] != "yell")
			fprintf(stderr,"Unknown command: [%s].\n",parm[0].c_str());
		exit(0);
		//seems useless
		return -1;
	}
	return 0;
}

int Shell::EXEC(string input,int fd){
	if(input.empty() || isWhitespace(input)){
		cout << "% ";
		fflush(stdout);
		return 0;
	}
	input.erase(remove(input.begin(), input.end(), '\n'),input.end());
	input.erase(remove(input.begin(), input.end(), '\r'),input.end());
	if(CheckPIPE(input,fd)  == -1){
		/* exit */
		return -1;
	}
	cout << "% ";
	fflush(stdout);
	return 0;
}
