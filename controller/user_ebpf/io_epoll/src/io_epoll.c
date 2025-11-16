#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>


#define MAX_EVENTS 10
#define PORT 8080

int set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;

    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int ioepoll_listener() {
    struct sockaddr_in addr;
    int fd = socket(AF_INET, SOCK_STREAM, 0);

    if (fd < 0) {
        perror("socket");
        return -1;
    }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind failed");//put in func
        close(fd);
        return -1;
    }

    if (listen(fd, 128) < 0) {
        perror("listen failed");
        close(fd);
        return -1;
    }

    if (set_nonblocking(fd) < 0) {
        perror("set_nonblocking");
        close(fd);
        return -1;
    }

    return fd;
}

void ioepoll_run_loop(int listen_fd) {
    int epoll_fd = epoll_create1(0);
    struct epoll_event ev;

    if (epoll_fd < 0) {
        perror("epoll_create1 failed");
        close(listen_fd);
        return;
    }

    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl failed");
        close(listen_fd);
        close(epoll_fd);
        return;
    }

    struct epoll_event events[MAX_EVENTS];
    printf("epoll listening on port %d\n", PORT);

    while (1) {
        int n = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (n < 0) {
            perror("epoll_wait failed");
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = events[i].data.fd;

            if (fd == listen_fd) {//new connection
                int client_fd = accept(listen_fd, NULL, NULL);
                if (client_fd >= 0) {
                    set_nonblocking(client_fd);

                    struct  epoll_event cev;
                    cev.events = EPOLLIN | EPOLLRDHUP;
                    cev.data.fd = client_fd;
                    
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &cev);
                    printf("New connection accepted: fd %d\n", client_fd);
                } else {
                    perror("accept failed");
                }
            }
            else{//client sent data
                char buf[1024];
                int r = recv(fd, buf, sizeof(buf), 0);

                if (r <= 0) {
                    printf("service disconnected, fd: %d\n", fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
                    close(fd);
                } else {
                    printf("Received %d bytes from fd: %d\n", r, fd);
                }
            }
        }
    }
}