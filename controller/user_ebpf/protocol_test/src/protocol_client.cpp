#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <thread>
#include <dlfcn.h>
#include <string>
#include <sys/un.h>
#include <unordered_map>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <cstring>

#include "protocol_decoder.h"
#include "protocol_encoder.h"
#include "service.pb.h"
#include "protocol_types.h"
#include <monitor.h>

// מבנה לשמירת החיבורים
struct tcp_session {
    uint32_t client_ip;
    uint16_t client_port;
    uint32_t server_seq;
    uint32_t client_seq;
    bool established;
};

// טבלת הסשנים
std::unordered_map<std::string, tcp_session> sessions;

// מבנה עזר לחישוב Checksum של TCP (דורש Pseudo Header)
struct pseudo_header {
    uint32_t source_address;
    uint32_t dest_address;
    uint8_t placeholder;
    uint8_t protocol;
    uint16_t tcp_length;
};

uint16_t calculate_checksum(uint16_t *ptr, int nbytes) {
    long sum = 0;
    uint16_t oddbyte;
    uint16_t answer;

    while (nbytes > 1) {
        sum += *ptr++;
        nbytes -= 2;
    }
    if (nbytes == 1) {
        oddbyte = 0;
        *((u_char*)&oddbyte) = *(u_char*)ptr;
        sum += oddbyte;
    }

    sum = (sum >> 16) + (sum & 0xffff);
    sum += (sum >> 16);
    answer = (uint16_t)~sum;
    
    return answer;
}

uint16_t compute_tcp_checksum(struct iphdr *pIph, struct tcphdr *pTcph) {
    struct pseudo_header psh;
    psh.source_address = pIph->saddr;
    psh.dest_address = pIph->daddr;
    psh.placeholder = 0;
    psh.protocol = IPPROTO_TCP;
    psh.tcp_length = htons(sizeof(struct tcphdr)); // אם יש DATA, צריך להוסיף את אורכו כאן

    int psize = sizeof(struct pseudo_header) + sizeof(struct tcphdr);
    char *pseudogram = (char*)malloc(psize);

    memcpy(pseudogram, (char*)&psh, sizeof(struct pseudo_header));
    memcpy(pseudogram + sizeof(struct pseudo_header), pTcph, sizeof(struct tcphdr));

    uint16_t checksum = calculate_checksum((uint16_t*)pseudogram, psize);
    free(pseudogram);
    return checksum;
}

void send_tcp_packet(int sock, const tcp_session& s, uint8_t flags) {
    char packet[4096] = {0};
    
    struct iphdr* ip = (struct iphdr*)packet;
    struct tcphdr* tcp = (struct tcphdr*)(packet + sizeof(struct iphdr));

    // בניית ה-IP Header
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = 0;
    ip->tot_len = sizeof(struct iphdr) + sizeof(struct tcphdr);
    ip->id = htons(rand() % 65535);
    ip->frag_off = 0;
    ip->ttl = 64;
    ip->protocol = IPPROTO_TCP;
    ip->saddr = inet_addr("10.0.0.100"); // ה-VIP (כדי שהלקוח יזהה אותנו)
    ip->daddr = s.client_ip;
    ip->check = calculate_checksum((uint16_t*)ip, sizeof(struct iphdr));

    // בניית ה-TCP Header
    tcp->source = htons(80); // הפורט המקורי שהלקוח פנה אליו
    tcp->dest = htons(s.client_port);
    tcp->seq = htonl(s.server_seq);
    tcp->ack_seq = htonl(s.client_seq);
    tcp->doff = 5; // גודל הכותרת (5 מילים של 32 ביט)
    
    if (flags & 0x02) tcp->syn = 1;
    if (flags & 0x10) tcp->ack = 1;

    tcp->window = htons(5840); // גודל חלון סטנדרטי
    tcp->check = 0;
    tcp->check = compute_tcp_checksum(ip, tcp);

    struct sockaddr_in dst = {};
    dst.sin_family = AF_INET;
    dst.sin_addr.s_addr = s.client_ip;

    if (sendto(sock, packet, ip->tot_len, 0, (struct sockaddr*)&dst, sizeof(dst)) < 0) {
        std::cerr << "Failed to send raw packet" << std::endl;
    }
}

void start_tcp_receiver(uint16_t my_assigned_port) {
    int raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (raw_sock < 0) {
        std::cerr << "Failed to create raw socket. Are you root?" << std::endl;
        return;
    }

    // קריטי: מודיעים לקרנל שאנחנו בונים את ה-IP Header בעצמנו
    int one = 1;
    if (setsockopt(raw_sock, IPPROTO_IP, IP_HDRINCL, &one, sizeof(one)) < 0) {
        std::cerr << "Failed to set IP_HDRINCL" << std::endl;
        return;
    }

    std::cout << "TCP Raw receiver started on port " << my_assigned_port << std::endl;

    while (true) {
        char buffer[65536];
        struct sockaddr_in saddr;
        socklen_t saddr_len = sizeof(saddr);
        
        int len = recvfrom(raw_sock, buffer, sizeof(buffer), 0, (struct sockaddr*)&saddr, &saddr_len);
        if (len < 0) continue;

        struct iphdr* ip = (struct iphdr*)buffer;
        struct tcphdr* tcp = (struct tcphdr*)(buffer + ip->ihl * 4);

        // std::cout << "[DEBUG] dst_ip=" << inet_ntoa(*(in_addr*)&ip->daddr) << ", dst_port=" << ntohs(tcp->dest) << ", src_ip=" << inet_ntoa(*(in_addr*)&ip->saddr) << ", src_port=" << ntohs(tcp->source) << std::endl;
        // // 1. האם הפקטה מיועדת ל-VIP?
        // if (ip->daddr != inet_addr("10.0.0.100")) continue;
        //std::cout<<"[DEBUG] backend port is: "<<tcp->dest<<std::endl;

        // 2. קריטי!! האם ה-XDP ניתב את הפקטה הזו לפורט הספציפי שלי?
        if (tcp->dest != htons(my_assigned_port)) continue;

        std::string key = std::to_string(ip->saddr) + ":" + std::to_string(ntohs(tcp->source));

        if (tcp->fin){
            std::cout << "[TCP] FIN received from " << inet_ntoa(*(in_addr*)&ip->saddr) << ":" << ntohs(tcp->source) << std::endl;

            auto it = sessions.find(key);
            if(it != sessions.end()){
                it->second.client_seq = ntohl(tcp->seq) + 1;
                //it->second.client_ack = ntohl(tcp->ack);
                send_tcp_packet(raw_sock, it->second, 0x10);

                std::cout << "[TCP] sent ACK for FIN\n";

                send_tcp_packet(raw_sock, it->second, 0x11);
                std::cout << "[TCP] sent FIN to client\n";
                sessions.erase(it);

                it->second.server_seq += 1;
            }
        }
        // טיפול ב-SYN
        else if (tcp->syn && !tcp->ack) {
            tcp_session s;
            s.client_ip = ip->saddr;
            s.client_port = ntohs(tcp->source);
            s.client_seq = ntohl(tcp->seq);
            s.server_seq = rand();
            s.established = false;

            sessions[key] = s;

            // שליחת SYN-ACK
            s.client_seq += 1; // מצפים ל-seq הבא מהלקוח
            send_tcp_packet(raw_sock, s, 0x12); // Flags: SYN=1, ACK=1
            std::cout << "[TCP] Sent SYN-ACK to " << key << std::endl;
        }
        
        // טיפול ב-ACK (סיום Handshake)
        else if (tcp->ack && !tcp->syn) {
            auto it = sessions.find(key);
            if (it != sessions.end() && !it->second.established) {
                it->second.established = true;
                it->second.server_seq += 1;
                std::cout << "[TCP] Connection established with " << key << std::endl;
            }
            
            // טיפול ב-DATA (אם יש פלואוד)
            int header_len = (ip->ihl * 4) + (tcp->doff * 4);
            int payload_len = len - header_len;
            
            if (it != sessions.end() && it->second.established && payload_len > 0) {
                it->second.client_seq += payload_len;
                // שליחת ACK על ה-Data
                send_tcp_packet(raw_sock, it->second, 0x10); // Flag: ACK=1
                std::cout << "[TCP] Acked data from " << key << std::endl;
            }
        }
        
    }
    close(raw_sock);
}

void start_udp_receiver(uint16_t port, const std::string& my_ip) {
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    //setsockopt(sock, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));//for last check

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    //addr.sin_addr.s_addr = INADDR_ANY;
    inet_pton(AF_INET, "10.0.0.100", &addr.sin_addr);//change to vip by service

    if (bind(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "[UDP] Failed to bind to " << my_ip << ":" << port << " (Check if IP exists)\n";//change to 80
        return;
    }

    std::cout << "[UDP] Receiver ready on " << my_ip << ":" << port << "\n";

    char buffer[4096];
    while (true) {
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int n = recvfrom(sock, buffer, sizeof(buffer), 0, (sockaddr*)&client_addr, &addr_len);

        if (n > 0) {
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            std::cout << "\n[UDP] >>>DSR SUCCESS! Packet from " << client_ip << " received at Backend (" << my_ip << ") <<<" << std::endl;
            std::cout<<"receivd packet, destination ip in IPIP header was: " << inet_ntoa(client_addr.sin_addr)<<std::endl;//change to my_ip!!
            std::string response = "ACK from backend " + my_ip + "\n";
            sendto(sock, response.c_str(), response.length(), 0, (struct sockaddr*)&client_addr, addr_len);
            std::cout << "[UDP] response sent directly back to: " << client_ip << std::endl;
        }
    }
}

void handle_server_messages(int sock, std::string& token, const std::string& my_ip) {
    uint8_t buffer[4096];
    lb::monitor::Monitor resource_monitor;

    resource_monitor.get_current_metrics();
    while (true) {
        int n = recv(sock, buffer, sizeof(buffer), 0);
        if (n <= 0) break;

        size_t consumed = 0;
        lb::protocol::MessageType type;
        std::vector<uint8_t> payload;

        while (consumed < (size_t)n) {
            auto result = lb::protocol::decoder::try_decode_frame(buffer + consumed, n - consumed, consumed, type, payload);
            if (result != lb::protocol::DecodeResult::OK) break;

            if (type == lb::protocol::MessageType::KEEPALIVE_REQ) {
                lb::KeepAliveResp resp;
                resp.set_session_token(token);
                auto bytes = lb::protocol::encoder::encode_keepalive_resp(resp);
                send(sock, bytes.data(), bytes.size(), 0);
            }
            else if (type == lb::protocol::MessageType::GET_REPORTS_REQ) {
                auto metrics = resource_monitor.get_current_metrics();
                if(metrics.cpu_usage > 50){
                    std::cout << "[WARNING!!!]" << std::endl;
                }
                if(my_ip != "127.0.0.5"){
                    metrics.memory_usage = 90;
                    metrics.cpu_usage = 90;
                }

                lb::ServiceReport report;
                report.set_session_token(token);
                report.set_cpu_usage(metrics.cpu_usage);
                report.set_memory_usage(metrics.memory_usage);
                std::cout << "[REPORT] sending metrics: cpu=" << metrics.cpu_usage << ", mem="<< metrics.memory_usage << std::endl;
                auto bytes = lb::protocol::encoder::encode_report(report);
                send(sock, bytes.data(), bytes.size(), 0);
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ./protocol_client <local_ip_to_bind> [unix_socket_path]\n";
        return 1;
    }
    std::string my_ip = argv[1];
    std::string path = (argc >= 4) ? argv[3] : "";

    int i = std::stoi(argv[2]);
    uint16_t port = 9000 + i;
    std::thread udp_thread(start_udp_receiver, port, my_ip);
    udp_thread.detach();

    std::thread tcp_thread(start_tcp_receiver, port);
    tcp_thread.detach();
    sleep(1);

    int sock;
    if(!path.empty()){
        sock = socket(AF_UNIX, SOCK_STREAM, 0);
        struct sockaddr_un addr = {};
        addr.sun_family = AF_UNIX;
        strncpy(addr.sun_path, path.c_str(), sizeof(addr.sun_path) - 1);

        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            perror("Could not connect to Controller with UNIX socket");
            close(sock);
            return 1;
        }
        std::cout << "[CLIENT] Connected to Controller with UNIX socket: " << path << "\n";
    }
    else {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in local_addr = {};
        local_addr.sin_family = AF_INET;
        local_addr.sin_port = 0;
        if(inet_pton(AF_INET, my_ip.c_str(), &local_addr.sin_addr) <= 0){
            std::cerr << "Invlaid local ip address\n";
            return 1;
        }

        //local_addr.sin_addr.s_addr = INADDR_ANY;
        if(bind(sock, (sockaddr*)&local_addr, sizeof(local_addr)) < 0){
            perror("bind TCP sokcet failed");
            close(sock);
            return 1;
        }

        sockaddr_in addr = {};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(8080);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(sock, (sockaddr*)&addr, sizeof(addr)) < 0) {
            std::cerr << "Could not connect to Controller\n";
            close(sock);
            return 1;
        }
    }

    lb::InitRequest req;
    req.set_service_name("serviceA");
    req.set_backend_ip(my_ip);
    req.set_udp_port(port);
    
    auto bytes = lb::protocol::encoder::encode_init_request(req);
    if(send(sock, bytes.data(), bytes.size(), 0) < 0){
        std::cerr << "Failed to send InitRequest\n";
        perror("send");
        close(sock);
        return 1;
    }

    uint8_t res_buf[1024];
    int n = recv(sock, res_buf, sizeof(res_buf), 0);
    size_t cons = 0; lb::protocol::MessageType t; std::vector<uint8_t> p;
    std::string token = "none";
    
    if (lb::protocol::decoder::try_decode_frame(res_buf, n, cons, t, p) == lb::protocol::DecodeResult::OK) {
        lb::InitResponse resp;
        resp.ParseFromArray(p.data(), p.size());
        token = resp.session_token();
        std::cout << "[CLIENT] Registered as " << my_ip << ". Token: " << token << "\n";
    }

    handle_server_messages(sock, token, my_ip);
    close(sock);
    return 0;
}