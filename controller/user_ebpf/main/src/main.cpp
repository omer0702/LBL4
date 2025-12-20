#include "io_epoll.h"
#include "protocol_encoder.h"
#include "service.pb.h"
#include <iostream>

using namespace lb;

int main() {
    uint16_t port = 8080;
    int listen_fd = lb::io_epoll::start_listen(port);
    if (listen_fd < 0) return 1;


    lb::io_epoll::run_loop(listen_fd);
    return 0;
}
