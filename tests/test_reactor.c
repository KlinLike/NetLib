#include "server.h"
#include "logger.h"
#include <string.h>
#include <stdlib.h>

static int test_echo_handler(char *msg, int length, char *response) {
    if (msg == NULL || response == NULL || length <= 0) {
        return 0;
    }
    int copy_len = length;
    if (copy_len > BUF_LEN) {
        copy_len = BUF_LEN;
    }
    memcpy(response, msg, copy_len);
    return copy_len;
}

int main(int argc, char *argv[]) {
    int port = 8888;
    if (argc > 1) {
        port = atoi(argv[1]);
    }
    log_server("Starting reactor test server on port %d...", port);
    return reactor_mainloop(port, 1, test_echo_handler);
}