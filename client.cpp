//////////////
// MAC OS X //
//////////////
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <iostream>
#include <fcntl.h>

#define UP "\x1b[1A"
#define ERASE_LINE "\x1b[2K"
#define MSG_NOSIGNAL 0x2000
#define MAXLINE 1024
#define max(a, b) a > b ? a : b;
#define __ERROR__(x) if (x < 0){ \
                        std::cerr << strerror(errno) << std::endl;\
                        return 1;\
                     }

void fillSockAddr(sockaddr_in& SockAddr)
{
    bzero(&SockAddr, sizeof(SockAddr));
    SockAddr.sin_family = AF_INET;
    SockAddr.sin_port = htons(3100);
    SockAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
}

void set_zeros(char *str, size_t len)
{
    for (uint i = 0; i < len; i++)
        *(str + i) = 0;
}

int main()
{
    int err;
    int ClientSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    struct sockaddr_in SockAddr;
    fillSockAddr(SockAddr);
    
    fd_set Set;
    FD_ZERO(&Set);
    err = connect(ClientSocket, (struct sockaddr*) &SockAddr, sizeof (SockAddr));
    __ERROR__(err);
    
    while (true)
    {
        FD_SET(ClientSocket, &Set);
        FD_SET(fileno(stdin), &Set);
        int Max = max(ClientSocket, fileno(stdin));
        select(Max + 1, &Set, NULL, NULL, NULL);
        
        if (FD_ISSET(ClientSocket, &Set))
        {
            static char recv_mes[MAXLINE];
            set_zeros(recv_mes, MAXLINE);
            if (recv(ClientSocket, recv_mes, MAXLINE, MSG_NOSIGNAL) <= 0){
                shutdown(ClientSocket, SHUT_RDWR);
                close(ClientSocket);
                return 1;
            }
            std::cout << recv_mes;
        }
        
        if (FD_ISSET(fileno(stdin), &Set))
        {
            static char send_mes[MAXLINE];
            
            fgets(send_mes, MAXLINE, stdin);
            std::cout << UP << ERASE_LINE;
            //SEND MES FROM STDIN
            send(ClientSocket, send_mes, strlen(send_mes), MSG_NOSIGNAL);
            //SECOND TEST
            //send(ClientSocket, "test-test\n", 11, MSG_NOSIGNAL);
        }
    }
    shutdown(ClientSocket, SHUT_RDWR);
    close(ClientSocket);
    return 0;
}
