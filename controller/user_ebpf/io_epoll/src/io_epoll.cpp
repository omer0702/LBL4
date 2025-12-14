#include "io_epoll.h"

#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <map>

#include "protocol_decoder.h"
#include "protocol_encoder.h"

namespace lb::io_epoll {

static bool running = true;

static int make_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int start_listen(uint16_t port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { perror("socket"); return -1; }

    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 128) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    if (make_nonblocking(fd) < 0) {
        perror("fcntl");
        close(fd);
        return -1;
    }

    return fd;
}

ssize_t send_all(int fd, const uint8_t* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        ssize_t n = send(fd, data + total, len - total, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                usleep(1000);
                continue;
            }
            return -1;
        }
        total += (size_t)n;
    }
    return (ssize_t)total;
}

ssize_t send_all(int fd, const std::vector<uint8_t>& bytes) {
    return send_all(fd, bytes.data(), bytes.size());
}

void stop_loop() {
    running = false;
}

void run_loop(int listen_fd, MessageHandler handler) {
    const int MAX_EVENTS = 64;
    int epfd = epoll_create1(0);
    if (epfd < 0) { perror("epoll_create1"); return; }

    epoll_event ev{};
    ev.events = EPOLLIN;
    ev.data.fd = listen_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, listen_fd, &ev) < 0) {
        perror("epoll_ctl add listen");
        close(epfd);
        return;
    }

    std::unordered_map<int, std::vector<uint8_t>> conn_buf;

    epoll_event events[MAX_EVENTS];

    std::cout << "[EPOLL] Listening...\n";

    while (running) {
        int n = epoll_wait(epfd, events, MAX_EVENTS, 1000);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }
        if (n == 0) continue;

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t ev_flags = events[i].events;

            if (fd == listen_fd) {
                while (true) {
                    int client = accept(listen_fd, nullptr, nullptr);
                    if (client < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }
                    make_nonblocking(client);
                    epoll_event cev{};
                    cev.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP | EPOLLERR;
                    cev.data.fd = client;
                    if (epoll_ctl(epfd, EPOLL_CTL_ADD, client, &cev) < 0) {
                        perror("epoll_ctl add client");
                        close(client);
                        continue;
                    }
                    conn_buf.emplace(client, std::vector<uint8_t>{});
                    std::cout << "[EPOLL] New client: fd=" << client << "\n";
                }
            } else {
                if (ev_flags & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                    std::cout << "[EPOLL] Client disconnected: fd=" << fd << "\n";
                    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                    close(fd);
                    conn_buf.erase(fd);
                    continue;
                }

                if (ev_flags & EPOLLIN) {
                    bool closed = false;
                    while (true) {
                        uint8_t tmp[4096];
                        ssize_t r = recv(fd, tmp, sizeof(tmp), 0);
                        if (r < 0) {
                            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                            if (errno == EINTR) continue;
                            perror("recv");
                            closed = true;
                            break;
                        } else if (r == 0) {
                            closed = true;
                            break;
                        } else {
                            auto &buf = conn_buf[fd];
                            buf.insert(buf.end(), tmp, tmp + r);
                        }
                    }
                    if (closed) {
                        std::cout << "[EPOLL] Client closed connection: fd=" << fd << "\n";
                        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                        close(fd);
                        conn_buf.erase(fd);
                        continue;
                    }

                    auto it = conn_buf.find(fd);
                    if (it == conn_buf.end()) continue;
                    auto &buffer = it->second;

                    while (!buffer.empty()) {
                        size_t consumed = 0;
                        MessageType type;
                        std::vector<uint8_t> payload;

                        DecodeResult res = lb::protocol::decoder::try_decode_frame(
                            buffer.data(),
                            buffer.size(),
                            consumed,
                            type,
                            payload
                        );

                        if (res == DecodeResult::OK) {
                            try {
                                handler(fd, type, payload);
                            } catch (const std::exception &ex) {
                                std::cerr << "[EPOLL] handler threw: " << ex.what() << "\n";
                            }

                            if (consumed <= buffer.size()) {
                                buffer.erase(buffer.begin(), buffer.begin() + consumed);
                            } else {
                                buffer.clear();
                                break;
                            }
                            continue;
                        } else if (res == DecodeResult::NEED_MORE_DATA) {
                            break;
                        } else {
                            std::cerr << "[EPOLL] decode error (invalid header?) on fd=" << fd << ", dropping\n";
                            epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
                            close(fd);
                            conn_buf.erase(fd);
                            break;
                        }
                    }
                } 
            }
        }
    } 

    for (auto &p : conn_buf) {
        close(p.first);
    }
    close(epfd);
}

}
