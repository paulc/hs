
int pipe_fd(char *cmd, int fd_in, int fd_out);
int pipe_fd_select(char *cmd, int fd_in, int fd_out);
void select_fds(int fds[4]);
