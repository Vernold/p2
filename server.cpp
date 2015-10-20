//////////////
// MAC OS X //
//////////////
#include <iostream>
#include <cstring>
#include <list>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/time.h>
#include <sys/event.h>
#include <errno.h>

#define MSG_NOSIGNAL 0x2000
#define MAXLINE 1024
#define NEVENTS 1000
#define __ERROR__(x) if (x < 0){ \
                        std::cerr << strerror(errno) << std::endl;\
                        return 1;\
                       }
#define __EV_ERROR__(x) std::cerr << "EV_ERROR: " << strerror(x) << std::endl;

using std::endl;
using std::cerr;
using std::cout;
using std::string;
using std::list;

typedef unsigned int uint;

class Client
{
    int fd;    // USER'S DESCRIPTOR
    string ip;  // IP ADDRESS
    string mes;  // THE NEXT MESSAGE TO SEND
public:
    Client(int f = 0, string i = "", string m = ""): fd(f), ip(i), mes(i)
    {}
    
    int get_fd() { return fd; }
    void set_fd(int fd) { this->fd = fd; }
    
    string get_ip() { return ip; }
    void set_ip(struct sockaddr_in *sa){
        char tmp[20];
        inet_ntop(sa->sin_family, &sa, tmp, MAXLINE);
        ip = "[" + string(tmp) + "]:";
    }
    
    string get_mes() { return mes; }
    void set_mes(string s) { mes = s; }
    void append_mes(string s) { mes += s; } //CONCATENATE PIECES OF THE FINAL MESSAGE
    void sendall(list<Client> clients)
    {
        for (auto it = clients.begin(); it!= clients.end(); it++)
        {
            if (fd != it->get_fd()){
                //SEND TO ALL USERS
                send(it->get_fd(), ip.c_str(), strlen(ip.c_str()), MSG_NOSIGNAL);
                send(it->get_fd(), mes.c_str(), strlen(mes.c_str()), MSG_NOSIGNAL);
            }
            else{
                //ECHO
                string my_ip = "[127.0.0.1]:";
                send(fd, my_ip.c_str(), strlen(my_ip.c_str()), MSG_NOSIGNAL);
                send(fd, mes.c_str(), strlen(mes.c_str()), MSG_NOSIGNAL);
            }
        }
    }
};

int set_nonblock(int fd)
{
    int flags;
#if defined(O_NONBLOCK)
    if (-1 == (flags = fcntl(fd, F_GETFL, 0))) flags = 0;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
    flags = 1;
    return ioctl(fd, FIOBIO, &flags);
#endif
}

void fillSockAddr(sockaddr_in& SockAddr)
{
    bzero(&SockAddr, sizeof(SockAddr));
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(3100);
    SockAddr.sin_addr.s_addr = htonl(INADDR_ANY);
}

int set_change(int kq, struct kevent *ch, int fd, int filter, int action)
{
    static int nchanges = 0;
    
    bzero(&ch[nchanges], sizeof(ch));
    EV_SET(&ch[nchanges], fd, filter, action, 0, 0, 0);
    kevent(kq, &ch[nchanges], 1, NULL, 0, NULL);
    return ++nchanges;
}

bool hasEOL(string str)
{
    for (auto it = str.begin(); it!= str.end(); it++)
    {
        if (*it == '\n')
            return true;
    }
    return false;
}

//SPLIT MESSAGES AND GET THEM READY
string handle_mes(string &str)
{
    std::size_t pos = str.find('\n');
    string mes = str.substr(0, pos + 1);
    str.erase(0, pos + 1);
    return mes;
}


list<Client>::iterator find_client(list<Client>::iterator b, list<Client>::iterator e, int fd)
{
    for (auto it = b; it!= e; it++)
    {
        if (it->get_fd() == fd) return it;
    }
    return b;
}

void close_passive_clients(list<Client> clients)
{
    for (auto it = clients.begin(); it!= clients.end(); it++)
    {
        cout << "[LOG]:" << it->get_ip() << "connection terminated" << endl;
        shutdown(it->get_fd(), SHUT_RDWR);
        close(it->get_fd());
    }
}

void set_zeros(char *str, size_t len)
{
    for (uint i = 0; i < len; i++)
        *(str + i) = 0;
}

int main()
{
    list<Client> clients;
    int err;

    struct sockaddr_in SockAddr;
    int MasterSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    fillSockAddr(SockAddr);
    
    int optval = 1;
    setsockopt(MasterSocket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    
    err = bind(MasterSocket, (sockaddr *) &SockAddr, sizeof(SockAddr));
    __ERROR__(err);
    
    err = listen(MasterSocket, SOMAXCONN);
    __ERROR__(err);
    
    int kq = kqueue();
    __ERROR__(kq);
    
    struct kevent Events[NEVENTS];
    struct kevent Changes[NEVENTS];
    struct timespec timer = {180, 0};
    
    err = set_change(kq, Changes, MasterSocket, EVFILT_READ, EV_ADD);
    __ERROR__(err);
    
    while(true)
    {
        int N = kevent(kq, NULL, 0, Events, NEVENTS, &timer);
        __ERROR__(N);
        
        if (N == 0)
        {
            close_passive_clients(clients);
            clients.clear();
        }
        if (N > 0)
        {
            for (uint i = 0; i < N; i++)
            {
                if (Events[i].flags & EV_ERROR)
                {
                    __EV_ERROR__(Events[i].data);
                }
                if (Events[i].ident == MasterSocket && Events[i].flags & EVFILT_READ)
                {
                    struct sockaddr_in ClientSockAddr;
                    socklen_t socklen = sizeof(ClientSockAddr);
    
                    int SlaveSocket = accept(MasterSocket, (struct sockaddr *)&ClientSockAddr, &socklen);
                    __ERROR__(SlaveSocket);
                    
                    err = set_nonblock(SlaveSocket);
                    __ERROR__(err);
                    
                    err = set_change(kq, Changes, SlaveSocket, EVFILT_READ, EV_ADD);
                    __ERROR__(err);
                    
                    //ADD NEW CLIENT
                    Client newClient = Client(SlaveSocket);
                    newClient.set_ip(&ClientSockAddr);
                    clients.push_back(newClient);
                    cout << "[LOG]:" << newClient.get_ip() << "accepted connection" << endl;
                    send(SlaveSocket, "Welcome!\n", 10, MSG_NOSIGNAL);
                }
                
                if (Events[i].ident != MasterSocket && Events[i].flags & EVFILT_READ)
                {
                    static char mes[MAXLINE];
                    set_zeros(mes, MAXLINE);
                    
                    auto sender = find_client(clients.begin(), clients.end(), Events[i].ident);
                    int result = recv(Events[i].ident, mes, MAXLINE, MSG_NOSIGNAL);
                    string str_mes = string(mes);
                    if (result <= 0){
                        cout << "[LOG]:" << sender->get_ip() << "connection terminated" << endl;
                        shutdown(Events[i].ident, SHUT_RDWR);
                        close(Events[i].ident);
                        clients.erase(find_client(clients.begin(), clients.end(), Events[i].ident));
                        continue;
                    }
                    //SPLIT AND COMPOSE FINAL MESSAGES
                    //WRITE THEM TO OUR LOG AND OTHER USERS
                    while (!str_mes.empty()){
                        if (hasEOL(str_mes)){
                            sender->append_mes(handle_mes(str_mes));
                            cout << "[LOG]:" <<sender->get_ip() << sender->get_mes();
                            sender->sendall(clients);
                            sender->set_mes("");
                        }
                        else{
                            sender->append_mes(str_mes);
                            str_mes.clear();
                        }
                    }
                }
            }
        }
    }
    shutdown(MasterSocket, SHUT_RDWR);
    close(MasterSocket);
    close(kq);
    return 0;
}
