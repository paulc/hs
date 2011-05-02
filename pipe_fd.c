
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

        int n;
        char *buf_in[BUF_LEN], *buf_out[BUF_LEN];
        int count_in = 0, count_out = 0;
        fd_set rfd,wfd;

        close(p_in[0]);
        close(p_out[1]);

        bzero(buf_in,BUF_LEN);
        bzero(buf_out,BUF_LEN);

        enum {FD_IN,FD_OUT,PIPE_IN,PIPE_OUT};
        int eof[4] = {0,0,0,0};
        int fds[4] = {fd_in,fd_out,p_in[1],p_out[0]};

        while (!eof[FD_OUT] || !eof[PIPE_OUT]) {
            FD_ZERO(&rfd);
            FD_ZERO(&wfd);

            if (!eof[FD_IN]) FD_SET(fds[FD_IN],&rfd);
            if (!eof[FD_OUT]) FD_SET(fds[FD_OUT],&wfd);
            if (!eof[PIPE_IN]) FD_SET(fds[PIPE_IN],&wfd);
            if (!eof[PIPE_OUT]) FD_SET(fds[PIPE_OUT],&rfd);

            if ((n = select(FD_SETSIZE,&rfd,&wfd,NULL,NULL)) < 0) {
                perror("Select failed");
                exit(-1);
            }

            //if (FD_ISSET(fds[FD_IN],&rfd)) fprintf(stderr,">>FD_IN readable\n");
            //if (FD_ISSET(fds[FD_OUT],&wfd)) fprintf(stderr,">>FD_OUT writable\n");
            //if (FD_ISSET(fds[PIPE_IN],&wfd)) fprintf(stderr,">>PIPE_IN writable\n");
            //if (FD_ISSET(fds[PIPE_OUT],&rfd)) fprintf(stderr,">>PIPE_OUT readable\n");

            if (FD_ISSET(fds[FD_IN],&rfd) && count_in < BUF_LEN) {
                /* Read from input */
                if ((n = read(fd_in,buf_in+count_in,BUF_LEN-count_in)) > 0) {
                    //fprintf(stderr,"Read from input: %d\n",n);
                    count_in += n;
                } else {
                    //fprintf(stderr,"Close FD_IN\n");
                    close(fds[FD_IN]);
                    eof[FD_IN] = 1;
                }
            }
            if (FD_ISSET(fds[PIPE_OUT],&rfd) && count_out < BUF_LEN) {
                /* Read from pipe */
                if ((n = read(p_out[0],buf_out+count_out,BUF_LEN-count_out)) > 0) {
                    //fprintf(stderr,"Read from pipe: %d\n",n);
                    count_out += n;
                } else {
                    //fprintf(stderr,"Close PIPE_OUT\n");
                    close(fds[PIPE_OUT]);
                    eof[PIPE_OUT] = 1;
                }
            }
            if (FD_ISSET(fds[PIPE_IN],&wfd) && count_in > 0) {
                /* Write to pipe */
                if ((n = write(p_in[1],buf_in,count_in)) > 0) {
                    //fprintf(stderr,"Wrote to pipe: %d\n",n);
                    count_in -= n;
                    if (count_in) {
                        memmove(buf_out,buf_in+n,count_in);
                    }
                } else {
                    //fprintf(stderr,"Close PIPE_IN\n");
                    close(fds[PIPE_IN]);
                    eof[PIPE_IN] = 1;
                }
            }
            if (FD_ISSET(fds[FD_OUT],&wfd) && count_out > 0) {
                /* Write to output */
                if ((n = write(fd_out,buf_out,count_out)) > 0) {
                    //fprintf(stderr,"Wrote to output: %d\n",n);
                    count_out -= n;
                    if (count_out) {
                        memmove(buf_out,buf_out+n,count_out);
                    } 
                } else {
                    //fprintf(stderr,"Close FD_OUT\n");
                    close(fds[FD_OUT]);
                    eof[FD_OUT] = 1;
                }
            }
            if (count_in == 0 && eof[FD_IN] && !eof[PIPE_IN]) {
                //fprintf(stderr,"FD_IN closed & buffer empty - close PIPE_IN\n");
                close(fds[PIPE_IN]);
                eof[PIPE_IN] = 1;
            }
            if (count_out == 0 && eof[PIPE_OUT] && !eof[FD_OUT]) {
                //fprintf(stderr,"PIPE_OUT closed & buffer empty - close FD_OUT\n");
                close(fds[FD_OUT]);
                eof[FD_OUT] = 1;
            }

            //fprintf(stderr,"-- EOF: FD_IN:%d FD_OUT:%d PIPE_IN:%d PIPE_OUT:%d\n",
            //            eof[FD_IN],eof[FD_OUT],eof[PIPE_IN],eof[PIPE_OUT]);

        }
    }
    return -1;
}

