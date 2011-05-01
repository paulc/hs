
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

