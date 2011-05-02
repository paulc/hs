
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pipe_fd.h"
#include "anet.h"

#define USAGE "Usage: hs [--cmd <cmd>] [--remote <dest>] [--port <port>] [--server] [--help]\n"

int main(int argc, char **argv) {

    char *cmd = "/bin/sh";
    char *p = NULL;
    int fd_in = 0, fd_out = 1;
    char *remote = NULL;
    char *bind = NULL;
    int port = 9876;
    int server = 0;

    static struct option longopts[] = {
        { "cmd",        required_argument,  NULL, 'c' },
        { "remote",     required_argument,  NULL, 'r' },
        { "port",       required_argument,  NULL, 'p' },
        { "server",     no_argument,        NULL, 's' },
        { "bind",       required_argument,  NULL, 'b' },
        { "help",       no_argument,        NULL, 'h' }
    };

    char ch;

    while ((ch = getopt_long(argc, argv, "c:r:p:sb:h", longopts, NULL)) != -1) {
        switch(ch) {
            case 'c':
                cmd = optarg;
                break;
            case 'r':
                if ((p = strchr(optarg,':')) != NULL) {
                    if ((port = strtol(p+1,(char **)NULL,10)) == 0) {
                        fprintf(stderr,"Invalid port\n");
                        exit(-1);
                    }
                    *p = 0;
                }
                remote = optarg;
                break;
            case 'p':
                if ((port = strtol(optarg,(char **)NULL,10)) == 0) {
                    fprintf(stderr,"Invalid port\n");
                    exit(-1);
                }
                break;
            case 's':
                server = 1;
                break;
            case 'b':
                bind = optarg;
                break;
            case 'h':
                fprintf(stderr,USAGE);
                exit(-1);
        }
    }


    char err[ANET_ERR_LEN];
    if (server) {
        char c_ip[1024];
        int s_fd,c_fd,c_port;

        if ((s_fd = anetTcpServer(err,port,bind)) == ANET_ERR) {
            fprintf(stderr,"Error creating server: %s",err);
            exit(-1);
        }
        if ((c_fd = anetAccept(err,s_fd,c_ip,&c_port)) == ANET_ERR) {
            fprintf(stderr,"Error accepting client connection: %s",err);
            exit(-1);
        }
        fprintf(stderr,"Connection from: %s port %d\n",c_ip,c_port);
        int fds[4] = {0,1,c_fd,c_fd};
        select_fds(fds);
    } else {
        if (remote) {
            if ((fd_in = fd_out = anetTcpConnect(err,remote,port)) == ANET_ERR) {
                fprintf(stderr,"Could not connect to server: %s",err);
                exit(-1);
            }
        }
        pipe_fd_select(cmd,fd_in,fd_out);
    }
    return 0;
}
