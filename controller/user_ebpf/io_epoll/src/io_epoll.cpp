#include "io_epoll.h"

#include <sys/epoll.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <cstring>
#include <map>
#include <sys/timerfd.h>

#include "protocol_decoder.h"
#include "protocol_encoder.h"
#include "session_manager.h"
#include "handlers.h"

namespace lb::io_epoll {

static bool running = true;
int timerfd;
int active_connections = 0;
const int MAX_CONNECTIONS = 1000;
int timer_tick_counter = 0;

void setup_timer(int epfd) {
    timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (timerfd == -1) {
        perror("timerfd_create");
        return;
    }

    itimerspec ts{};
    ts.it_interval.tv_sec = 1;
    ts.it_interval.tv_nsec = 0;
    ts.it_value.tv_sec = 1;
    ts.it_value.tv_nsec = 0;


    if (timerfd_settime(timerfd, 0, &ts, NULL) == -1) {
        perror("timerfd_settime");
        close(timerfd);
        return;
    }

    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = timerfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, timerfd, &ev);
}

void close_client(int fd, int epfd, std::unordered_map<int, std::vector<uint8_t>>& conn_buf, int& active_connections) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
    close(fd);
    active_connections--;
    conn_buf.erase(fd);
    lb::session::SessionManager::instance().remove_session(fd);
}

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

// ssize_t send_all(int fd, const uint8_t* data, size_t len) {
//     size_t total = 0;
//     while (total < len) {
//         ssize_t n = send(fd, data + total, len - total, 0);
//         if (n < 0) {
//             if (errno == EINTR) continue;
//             if (errno == EAGAIN || errno == EWOULDBLOCK) {
//                 usleep(1000);
//                 continue;
//             }
//             return -1;
//         }
//         total += (size_t)n;
//     }
//     return (ssize_t)total;
// }

// ssize_t send_all(int fd, const std::vector<uint8_t>& bytes) {
//     return send_all(fd, bytes.data(), bytes.size());
// }

void stop_loop() {
    running = false;
}

void choose_handler(int fd, MessageType type, const std::vector<uint8_t>& payload, int epfd,
                    std::unordered_map<int, std::vector<uint8_t>>& conn_buf,
                    int& active_connections) {
    HandlerResult res;

    switch (type) {
        case MessageType::INIT_REQ:
            res = lb::handlers::handle_init_req(fd, payload);
            break;
        case MessageType::KEEPALIVE_RESP:
            res = lb::handlers::handle_keepalive_resp(fd, payload);
            break;
        case MessageType::REPORT:
            //std::cout << "here1\n";
            res = lb::handlers::handle_get_report_resp(fd, payload);
            lb::session::SessionManager::instance().print_session_stats();
            //std::cout << "here3\n";
            break;
        case MessageType::CLOSE_REQ:
            res = lb::handlers::handle_close_req(fd, payload);
            break;
        default:
            std::cerr << "[EPOLL] Unknown message type received: fd=" << fd << "\n";
            break;
    }

    if(res == HandlerResult::CLOSE_CONNECTION){
        std::cout << "[EPOLL] Closing connection as per handler request: fd=" << fd << "\n";
        close_client(fd, epfd, conn_buf, active_connections);
    }
}

void send_keepalive_request(int fd) {
    lb::KeepAlive ka;
    ka.set_timestamp(static_cast<uint64_t>(time(nullptr)));
    ka.set_session_token(lb::session::SessionManager::instance().get_session_by_fd(fd)->token);
    
    auto bytes = lb::protocol::encoder::encode_keepalive_req(ka);
    lb::handlers::send_all(fd, bytes);
}

void send_get_reports_request(int fd) {
    lb::GetReport gr;
    gr.set_session_token(lb::session::SessionManager::instance().get_session_by_fd(fd)->token);

    auto bytes = lb::protocol::encoder::encode_get_reports_req(gr);
    lb::handlers::send_all(fd, bytes);
}


void run_loop(int listen_fd) {
    const int MAX_EVENTS = 64;
    int epfd = epoll_create1(0);
    if (epfd < 0) { perror("epoll_create1"); return; }

    setup_timer(epfd);

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
        int n = epoll_wait(epfd, events, MAX_EVENTS, -1);
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
                    if( active_connections >= MAX_CONNECTIONS) {//handle flood(DOS) problem
                        std::cerr << "[EPOLL] Max connections reached, rejecting new connections\n";
                        break;
                    }

                    int client = accept(listen_fd, nullptr, nullptr);
                    if (client < 0) {
                        if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                        perror("accept");
                        break;
                    }

                    active_connections++;
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
            } 
            else if(fd == timerfd){
                uint64_t expirations;//contains how many times the timer has "rung" since the last call(if >1, we missed some)
                read(timerfd, &expirations, sizeof(expirations));
                auto expired_fds = lb::session::SessionManager::instance().get_expired_sessions(20);
                for (int e_fd : expired_fds) {//handle zombie sessions problem(keepalive fail)
                    std::cout << "[EPOLL] Session expired due to inactivity: fd=" << e_fd << "\n";
                    close_client(e_fd, epfd, conn_buf, active_connections);
                }

                timer_tick_counter++;
                auto active_fds = lb::session::SessionManager::instance().get_all_session_fds();
                for( int a_fd : active_fds ) {
                    send_keepalive_request(a_fd);
                    if(timer_tick_counter % 5 == 0) {
                        send_get_reports_request(a_fd);
                    }
                }
            }
            else {
                if (ev_flags & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {//handle sudden disconnects problem
                    std::cout << "[EPOLL] Client disconnected: fd=" << fd << "\n";
                    close_client(fd, epfd, conn_buf, active_connections);
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
                            if(buf.size() > 1024 * 1024) {
                                std::cerr << "[EPOLL] Connection buffer overflow, closing fd=" << fd << "\n";
                                closed = true;
                                break;
                            }
                        }
                    }
                    if (closed) {
                        std::cout << "[EPOLL] Client closed connection: fd=" << fd << "\n";
                        close_client(fd, epfd, conn_buf, active_connections);
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
                        //std::cout << "type after decode: " << static_cast<uint16_t>(type) << "\n";
                        if (res == DecodeResult::OK) {
                            choose_handler(fd, type, payload, epfd, conn_buf, active_connections);

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