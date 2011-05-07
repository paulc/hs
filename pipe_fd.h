
#include <stdint.h>

int pipe_fd_select(char *cmd, int fd_in, int fd_out, uint8_t *xor_mask);
void select_fds(int fds[4],uint8_t *xor_mask);

