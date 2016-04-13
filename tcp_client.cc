/*==================================================================
*   Copyright (C) 2016 All rights reserved.
*   
*   filename:     tcp_client.cc
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
#include <sys/select.h>
#include <sys/wait.h>
#include <arpa/inet.h>

#define MAXSIZE     1024
#define IPADDRESS   "127.0.0.1"
#define SERV_PORT   8787
#define FDSIZE        1024
#define EPOLLEVENTS 20


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

void str_cli(FILE *fp, int sockfd) {
    char sendline[MAXSIZE], recvline[MAXSIZE];
    while(fgets(sendline, MAXSIZE, fp) != NULL) {
        write(sockfd, sendline, strlen(sendline));
        if(readLine(sockfd, recvline, MAXSIZE) == 0) {
            err_quit("str_cli:server terminated prematurely");
        }
        fputs(recvline, stdout);
    }
}

void str_cli_select(FILE *fp, int sockfd) {
    int maxfd1, stdlineof;
    fd_set rset;
    char sendline[MAXSIZE], recvline[MAXSIZE];
    stdlineof = 0;
    FD_ZERO(&rset);
    for(;;) {
        if(stdlineof == 0)
            FD_SET(fileno(fp), &rset);
        FD_SET(sockfd, &rset);
        int max = fileno(fp);
        if(sockfd > max)
            max = sockfd;
        maxfd1 = max + 1;
        select(maxfd1, &rset, NULL, NULL, NULL);
        if(FD_ISSET(sockfd, &rset)) {
            if(readLine(sockfd, recvline, MAXSIZE) == 0) {
                if(stdlineof == 1)
                    return;
                else
                    err_quit("str_cli_select: server terminated prematurely");
            }
            fputs(recvline, stdout);
        }

        if(FD_ISSET(fileno(fp), &rset)) {
            if(fgets(sendline, MAXSIZE, fp) == NULL) {
                stdlineof = 1;
                shutdown(sockfd, SHUT_WR); // send FIN
                FD_CLR(fileno(fp), &rset);
                continue;
            }
            write(sockfd, sendline,strlen(sendline));
        }
    }

}

int main(int argc, char* argv[]) {
    int sockfd;
    struct sockaddr_in servaddr;
    if(argc != 2) {
        err_quit("uage:tcpli <IPaddress>");
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(SERV_PORT);
    inet_pton(AF_INET, argv[1], &servaddr.sin_addr);
    connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    str_cli_select(stdin, sockfd);
    //str_cli(stdin, sockfd);
    exit(0);
    
}
