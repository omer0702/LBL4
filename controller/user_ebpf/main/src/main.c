#include <stdio.h>
#include "main.h"


int main(){
    printf("load balancer start\n");

    int listen_fd = ioepoll_listener();
    if(listen_fd < 0){
        fprintf(stderr, "failed to init listener\n");
        return 1;
    }

    ioepoll_run_loop(listen_fd);

    return 1;
}