
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <stdint.h>
#include <time.h>
#include <unistd.h>

#include "pipe_fd.h"

static void debug(char *format,...) {
    va_list ap;
    if (getenv("DEBUG")) {
        va_start(ap,format);
        vfprintf(stderr,format,ap);
        va_end(ap);
    }
}

void exec_child(char *cmd,int p_in[2],int p_out[2]) {
    dup2(p_in[0],0);
    dup2(p_out[1],1);
    dup2(p_out[1],2);
    close(p_in[0]);
    close(p_in[1]);
    close(p_out[0]);
    close(p_out[1]);
    execl("/bin/sh","sh","-c",cmd,NULL);
    perror("Exec failed");
    exit(255);
}

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
        exec_child(cmd,p_in,p_out);
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

int pipe_fd_select(char *cmd, int fd_in, int fd_out, uint8_t *xor_mask, int timeout) {
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
        exec_child(cmd,p_in,p_out);
    } else {
        /* Parent */
        close(p_in[0]);
        close(p_out[1]);
        int fds[4] = {fd_in,fd_out,p_in[1],p_out[0]};
        select_fds(fds,xor_mask,timeout);
    }
    return -1;
}

#define BUF_LEN 4096

void xor_block(char *data,int count,uint8_t *xor_mask, int *xor_index) {
    for (int i = 0; i < count; i++) {
        *(data + i) = *(data + i) ^ *(xor_mask + *xor_index);
        *xor_index = ((*xor_index) + 1) % 32;
    }
}

void select_fds(int fds[4],uint8_t *xor_mask,int timeout) {
    // {fd_in,fd_out,p_in[1],p_out[0]};
    int n;
    char *buf_in[BUF_LEN], *buf_out[BUF_LEN];
    int n_in = 0, n_out = 0;
    int  xor_rindex = 0, xor_windex = 0;
    int wait_timeout = 0;
    time_t start = time(NULL),now;
    fd_set rfd,wfd;
    bzero(buf_in,BUF_LEN);
    bzero(buf_out,BUF_LEN);

    enum {FD_IN,FD_OUT,PIPE_IN,PIPE_OUT};

    int eof[4] = {0,0,0,0};

    while (!eof[FD_OUT] || !eof[PIPE_OUT]) {

        FD_ZERO(&rfd);
        FD_ZERO(&wfd);

        if (!eof[FD_IN]) FD_SET(fds[FD_IN],&rfd);
        if (!eof[FD_OUT]) FD_SET(fds[FD_OUT],&wfd);
        if (!eof[PIPE_IN]) FD_SET(fds[PIPE_IN],&wfd);
        if (!eof[PIPE_OUT]) FD_SET(fds[PIPE_OUT],&rfd);

        if ((n = select(FD_SETSIZE,&rfd,&wfd,NULL,NULL)) < 0) {
            perror("Select failed");
            return;
        }

        n = 0;

        if (FD_ISSET(fds[FD_IN],&rfd) && n_in < BUF_LEN) {
            /* Read from input */
            if ((n = read(fds[FD_IN],buf_in+n_in,BUF_LEN-n_in)) > 0) {
                debug("Read from input: %d\n",n);
                if (xor_mask) {
                    xor_block((char *)(buf_in+n_in),n,xor_mask,&xor_rindex);
                }
                n_in += n;
            } else {
                debug("Close FD_IN\n");
                //close(fds[FD_IN]);
                eof[FD_IN] = 1;
            }
        }
        if (FD_ISSET(fds[PIPE_OUT],&rfd) && n_out < BUF_LEN) {
            /* Read from pipe */
            if ((n = read(fds[PIPE_OUT],buf_out+n_out,BUF_LEN-n_out)) > 0) {
                debug("Read from pipe: %d\n",n);
                if (xor_mask) {
                    xor_block((char *)(buf_out+n_out),n,xor_mask,&xor_windex);
                }
                n_out += n;
            } else {
                debug("Close PIPE_OUT\n");
                close(fds[PIPE_OUT]);
                eof[PIPE_OUT] = 1;
            }
        }
        if (FD_ISSET(fds[PIPE_IN],&wfd) && n_in > 0) {
            /* Write to pipe */
            if ((n = write(fds[PIPE_IN],buf_in,n_in)) > 0) {
                debug("Wrote to pipe: %d\n",n);
                n_in -= n;
                if (n_in) {
                    memmove(buf_out,buf_in+n,n_in);
                }
            } else {
                debug("Close PIPE_IN\n");
                close(fds[PIPE_IN]);
                eof[PIPE_IN] = 1;
            }
        }
        if (FD_ISSET(fds[FD_OUT],&wfd) && n_out > 0) {
            /* Write to output */
            if ((n = write(fds[FD_OUT],buf_out,n_out)) > 0) {
                debug("Wrote to output: %d\n",n);
                n_out -= n;
                if (n_out) {
                    memmove(buf_out,buf_out+n,n_out);
                } 
            } else {
                debug("Close FD_OUT\n");
                //close(fds[FD_OUT]);
                eof[FD_OUT] = 1;
            }
        }

        if (timeout > 0) {
            if (n > 0) {
                start = time(NULL);
            } else {
                now = time(NULL);
                if (difftime(now,start) > timeout) {
                    debug(">>> Timeout\n");
                    close(fds[PIPE_OUT]);
                    close(fds[PIPE_IN]);
                    close(fds[FD_OUT]);
                    close(fds[FD_IN]);
                    eof[PIPE_OUT] = 1;
                    eof[PIPE_IN] = 1;
                    eof[FD_OUT] = 1;
                    eof[FD_IN] = 1;
                }
            }
        }


        if (n_in == 0 && eof[FD_IN] && !eof[PIPE_OUT]) {
            debug("FD_IN closed & buffer empty - check for wait_timeout\n");
            usleep(10000);
            if (n == 0 && wait_timeout++ > 20) {
                close(fds[PIPE_OUT]);
                eof[PIPE_OUT] = 1;
            }
        }
        if (n_out == 0 && eof[PIPE_OUT] && !eof[FD_OUT]) {
            debug("PIPE_OUT closed & buffer empty - check for wait_timeout\n");
            usleep(10000);
            if (n == 0 && wait_timeout++ > 20) {
                eof[FD_OUT] = 1;
            }
        }
    }
}

