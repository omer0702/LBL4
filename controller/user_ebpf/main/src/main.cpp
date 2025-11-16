#include <iostream>
#include "main.h"
#include "io_epoll.h"
#include <cstdio>


int main(){
    std::cout << "load balancer starting..." << std::endl;

    int listen_fd = ioepoll_listener();
    if(listen_fd < 0){
        std::cerr << "Failed to set up listener." << std::endl;
        return 1;
    }

    ioepoll_run_loop(listen_fd);

    return 0;
}