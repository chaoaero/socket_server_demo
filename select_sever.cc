/*==================================================================
*   Copyright (C) 2016 All rights reserved.
*   
*   filename:     tcp_server.cc
*   author:       Meng Weichao
*   created:      2016/04/13
*   description:  
*
================================================================*/
#include <netinet/in.h>
#include <sys/socket.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "err_msg.h"
#include <stdlib.h>
#include <sys/epoll.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define MAXSIZE     1024
#define IPADDRESS   "127.0.0.1"
#define SERV_PORT   8787
#define FDSIZE        1024
#define EPOLLEVENTS 20
#define LISTENEQ 5


void sig_chld(int signo) {
    pid_t pid;
    int stat;
    //pid = wait(&stat);
    while((pid = waitpid(-1, &stat, WNOHANG)) > 0)
        printf("child %d terminated\n", pid);
    return;
}

ssize_t
readLine(int fd, void *buffer, size_t n)
{
    ssize_t numRead;                    /* # of bytes fetched by last read() */
    size_t totRead;                     /* Total bytes read so far */
    char *buf;
    char ch;

    if (n <= 0 || buffer == NULL) {
        errno = EINVAL;
        return -1;
    }

    buf = (char *)buffer;                       /* No pointer arithmetic on "void *" */

    totRead = 0;
    for (;;) {
        numRead = read(fd, &ch, 1);

        if (numRead == -1) {
            if (errno == EINTR)         /* Interrupted --> restart read() */
                continue;
            else
                return -1;              /* Some other error */

        } else if (numRead == 0) {      /* EOF */
            if (totRead == 0)           /* No bytes read; return 0 */
                return 0;
            else                        /* Some bytes read; add '\0' */
                break;

        } else {                        /* 'numRead' must be 1 if we get here */
            if (totRead < n - 1) {      /* Discard > (n - 1) bytes */
                totRead++;
                *buf++ = ch;
            }

            if (ch == '\n')
                break;
        }
    }

    *buf = '\0';
    return totRead;
}


void str_echo(int sockfd) {
    ssize_t n;
    char line[MAXSIZE];
    for(;;) {
        if((n = readLine(sockfd, line, MAXSIZE)) == 0 ) 
           return;
        fputs(line, stdout);
        write(sockfd, line, n);
    }
}

int main(int argc, char* argv[]) {
    int listenfd, connfd;
    int i, maxi, maxfd, sockfd;
    int nready, client[FD_SETSIZE];
    ssize_t n;
    fd_set rset, allset;
    char line[MAXSIZE];
    pid_t childpid;
    socklen_t clien;
    struct sockaddr_in cliaddr, servaddr;
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    //inet_pton(AF_INET,argv[1] ,&servaddr.sin_addr);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    listen(listenfd, LISTENEQ );

    signal(SIGCHLD, sig_chld);
    //signal(SIGPIPE, SIG_IGN);
    
    maxfd = listenfd;
    maxi = -1;
    for(i = 0; i < FD_SETSIZE; i++)
        client[i] = -1;
    FD_ZERO(&allset);
    FD_SET(listenfd, &allset);

    for(;;) {
        rset = allset;
        nready = select(maxfd + 1, &rset, NULL, NULL, NULL);
        if(FD_ISSET(listenfd, &rset)) {
            clien = sizeof(cliaddr);
            connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clien);
            for(i = 0; i< FD_SETSIZE; i++) {
                if(client[i] < 0) {
                    client[i] = connfd;
                    break;
                }
            }
            if(i == FD_SETSIZE)
                err_quit("too many clients");
            FD_SET(connfd, &rset);
            if(connfd > maxfd)
                maxfd = connfd;
            if(i > maxi)
                maxi = i;
            if(--nready <= 0)
                continue;
        }

        for(i = 0; i< FD_SETSIZE; i++) {
            if((sockfd = client[i]) < 0 ) 
                continue;
            if(FD_ISSET(sockfd, &rset)) {
                if((n == readLine(sockfd, line, MAXSIZE)) == 0) {
                    close(sockfd);
                    FD_CLR(sockfd, &allset);
                    client[i] = -1;
                } else
                    write(sockfd, line, n);
                if(--nready <= 0)
                    break;
                
            }
        }
    }

    return 0;
}
