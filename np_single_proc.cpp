#include "np_single_proc.h"

int passiveTCP(int qlen);
int echo(int fd);
void welcome(int fd);
int openshell(int fd);

int passiveTCP(int qlen, int port)
{
    struct sockaddr_in sin; /* an Internet endpoint address	*/
    int s, type;            /* socket descriptor and socket type	*/

    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(port);
    /* Allocate a socket */
    s = socket(PF_INET, SOCK_STREAM, 0);
    if (s < 0)
        perror("socket fail");

    /* Bind the socket */
    if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        perror("bind fail");
    if (listen(s, qlen) < 0)
        perror("listen fail");
    return s;
}

bool sortid(client s1, client s2){
   return s1.ID < s2.ID;
}

int main(int argc, char *argv[])
{
    struct sockaddr_in fsin; /* the from address of a client */
    int alen;                /* from-address length	*/
    int fd, nfds;
    int port = atoi(argv[1]);
    for(int i = 0;i < CLIENTMAX;++i) ID_arr[i] = 0;
    msock = passiveTCP(QLEN, port);
    nfds = getdtablesize();
    FD_ZERO(&afds);
    //exclude server fd
    FD_SET(msock, &afds);

    while (1)
    {
        memcpy(&rfds, &afds, sizeof(rfds));
        int err,stat;
        do{
            stat = select(nfds, &rfds, (fd_set *)0, (fd_set *)0, (struct timeval *)0);
            if( stat< 0) err = errno;
        } while ((stat < 0) && (err == EINTR)); /* blocked by pipe or other reason */
        if (stat < 0)
            perror("select fail");
        if (FD_ISSET(msock, &rfds))
        {
            int ssock;
            alen = sizeof(fsin);
            ssock = accept(msock, (struct sockaddr *)&fsin,
                           (socklen_t *)&alen);
            if (ssock < 0)
                perror("accept fail");
            /* Check number of clients*/
            int ID;
            ID = get_min_num();
            if(ID < 0){
                perror("Client MAX");
                continue;
            }
            FD_SET(ssock, &afds);
            /* send welcome msg */
            char ip[INET6_ADDRSTRLEN];
            sprintf(ip, "%s:%d", inet_ntoa(fsin.sin_addr), ntohs(fsin.sin_port));
            welcome(ssock);
            /* create user info */
            string tmp(ip);
            vector<npipe> np;
            np.clear();
            client c = {ID, tmp, "no name", ssock,np};
            client_info.push_back(c);
            sort(client_info.begin(), client_info.end(), sortid);
            /* broadcast login information */
            broadcast(0, "", &c, 0);
            write(ssock, "% ", 2);
        }
        for (fd = 0; fd < nfds; ++fd)
            if (fd != msock && FD_ISSET(fd, &rfds))
            {
                if (openshell(fd) == -1)
                {
                    /* broadcast logout information */
                    client c;
                    for(int i = 0;i < client_info.size();i++){
                        client tmp = client_info[i];
                        if(tmp.fd == fd){
                            c = tmp;
                            break;
                        }
                    }
                    broadcast(4, "", &c,0);
                    DeleteClient(fd);
                    close(1);
                    close(2);
                    close(fd);
                    dup2(0, 1);
                    dup2(0, 2);
                    FD_CLR(fd, &afds);
                }
            }
    }
}

void welcome(int fd)
{
    char buf[BUFSIZE] =
        "***************************************\n\
** Welcome to the information server **\n\
***************************************\n";
    int cc;
    cc = send(fd, buf, BUFSIZE, 0);
    return;
}

int openshell(int fd)
{
    char buf[BUFSIZE];
    bzero((char *)buf, BUFSIZE);
    int cc;
    cc = recv(fd, buf, BUFSIZE, 0);
    if (cc == 0)
        cout << "socket: " << fd << "closed" << endl;
    else if(cc < 0)
        perror("receive error");
    buf[cc] = '\0';
    string input(buf);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    Shell s;
    int status = s.EXEC(input, fd);
    return status;
    /*
    
    if (cc < 0)
        perror("echo read");
    if (cc && write(fd, buf, cc) < 0)
        perror("echo write");
    return cc;
    */
}