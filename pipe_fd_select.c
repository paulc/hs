
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <unistd.h>


int pipe_fd(char *cmd, int fd_in, int fd_out) {
    /* Connect cmd to fd */
    int p_in[2],p_out[2];
    pid_t pid;

    if (pipe(p_in) != 0 || pipe(p_out) != 0) {
        perror("Pipe failed");
        return -1;
    }
    pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return -1;
    } else if (pid == 0) {
        /* Child - exec cmd */
        close(fd_in);
        close(fd_out);
        dup2(p_in[0],0);
        dup2(p_out[1],1);
        close(p_in[0]);
        close(p_in[1]);
        close(p_out[0]);
        close(p_out[1]);
        execl("/bin/sh","sh","-c",cmd,NULL);
        perror("Exec failed");
        exit(255);
    } else {
        /* Parent */

        pid = fork();
        signal(SIGPIPE, SIG_IGN);

        if (pid < 0) {
            perror("Fork failed");
            return -1;
        } else if (pid == 0) {
            /* Child - handle write from fd_in to pipe */
            close(p_in[0]);
            close(p_out[0]);
            close(p_out[1]);
            if (fd_in != fd_out) {
                close(fd_out);
            }
            char c;
            while (read(fd_in,&c,1) > 0) {
                write(p_in[1],&c,1);
            }
            close(fd_in);
            close(p_in[1]);
            exit(0);
        } else {
            /* Parent - handle write from pipe to fd (f_out) */
            close(p_in[0]);
            close(p_in[1]);
            close(p_out[1]);
            if (fd_in != fd_out) {
                close(fd_in);
            }
            char c;
            while (read(p_out[0],&c,1) > 0) {
                write(fd_out,&c,1);
            }
            close(fd_out);
            close(p_out[0]);
            return 0;
        }
    }
    return -1;
}

void sigpipe_handler(int sig) {
    fprintf(stderr,"SIGPIPE\n");
}

int pipe_fd_select(char *cmd, int fd_in, int fd_out) {
    /* Connect cmd to fd */
    int p_in[2],p_out[2];
    pid_t pid;

    if (pipe(p_in) != 0 || pipe(p_out) != 0) {
        perror("Pipe failed");
        return -1;
    }
    pid = fork();

    if (pid < 0) {
        perror("Fork failed");
        return -1;
    } else if (pid == 0) {
        /* Child - exec cmd */
        dup2(p_in[0],0);
        dup2(p_out[1],1);
        close(p_in[0]);
        close(p_in[1]);
        close(p_out[0]);
        close(p_out[1]);
        execl("/bin/sh","sh","-c",cmd,NULL);
        perror("Exec failed");
        exit(255);
    } else {
        /* Parent */

#define BUF_LEN 4096

        int n, eof_in = 0, eof_out = 0;
        char *buf_in[BUF_LEN], *buf_out[BUF_LEN];
        int count_in = 0, count_out = 0;
        struct timeval t;
        fd_set rfd,wfd;

        close(p_in[0]);
        close(p_out[1]);

        t.tv_sec = 2;
        t.tv_usec = 100000;

        bzero(buf_in,BUF_LEN);
        bzero(buf_out,BUF_LEN);

        while (!eof_in || !eof_out) {
            FD_ZERO(&rfd);
            FD_ZERO(&wfd);
            if (!eof_in) {
                FD_SET(fd_in,&rfd);
            }
            if (!eof_out) {
                FD_SET(p_out[0],&rfd);
            }
            //FD_SET(fd_out,&wfd);
            //FD_SET(p_in[1],&wfd);
            if ((n = select(FD_SETSIZE,&rfd,&wfd,NULL,&t)) < 0) {
                perror("Select failed");
                exit(-1);
            }
            printf("-- eof_in=%d eof_out=%d\n",eof_in,eof_out);
            if (FD_ISSET(fd_in,&rfd)) printf(">>fd_in readable\n");
            if (FD_ISSET(p_out[0],&rfd)) printf(">>p_out[0] readable\n");
            if (FD_ISSET(p_in[1],&wfd)) printf(">>p_in[1] writable\n");
            if (FD_ISSET(fd_out,&wfd)) printf(">>fd_out writable\n");

            if (FD_ISSET(fd_in,&rfd) && count_in < BUF_LEN) {
                /* Read from input */
                if ((n = read(fd_in,buf_in+count_in,BUF_LEN-count_in)) < 0) {
                    close(fd_in);
                    eof_in = 1;
                } else {
                    count_in += n;
                }
                printf("Read from input: %d\n",count_in);
            }
            if (FD_ISSET(p_out[0],&rfd) && count_out < BUF_LEN) {
                /* Read from pipe */
                if ((n = read(p_out[0],buf_out+count_out,BUF_LEN-count_out)) < 0) {
                    printf("Close pipe\n");
                    close(p_out[0]);
                    eof_out = 1;
                } else {
                    count_out += n;
                }
                if (n>0)
                    printf("Read from pipe: %d\n",n);
            }
            if (FD_ISSET(p_in[1],&wfd) && count_in > 0) {
                printf("Pipe writable\n");
                /* Write to pipe */
                if ((n = write(p_in[1],buf_in,count_in)) < 0) {
                    perror("Cant write to pipe");
                    printf("Pipe write failed\n");
                    exit(-1);
                } else {
                    count_in -= n;
                    if (count_in) {
                        memmove(buf_out,buf_in+n,count_in);
                    }
                }
                printf("Wrote to pipe: %d\n",n);
            }
            if (FD_ISSET(fd_out,&wfd) && count_out > 0) {
                /* Write to output */
                if ((n = write(fd_out,buf_out,count_out)) < 0) {
                    perror("Cant write to output");
                    exit(-1);
                } else {
                    count_out -= n;
                    if (count_out) {
                        memmove(buf_out,buf_out+n,count_out);
                    }
                }
                printf("Wrote to output: %d\n",n);
            }
        }
    }
    return -1;
}

