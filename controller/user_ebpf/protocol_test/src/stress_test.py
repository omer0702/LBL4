import socket
import time
import statistics 
import struct
import random

#from model.backend.db.queries import save_performance_test

VIP = "10.0.0.100"
PORT = 80
MESSAGE = "Hello, World!"
NUM_REQUESTS = 100
TIMEOUT = 1.0

def stress_test():
    results = []
    latencies = []
    lost_packets = 0

    print(f"Starting stress test: sending {NUM_REQUESTS} UDP packets to {VIP}:{PORT}")
    for i in range(NUM_REQUESTS):
        src_port = 11111 + i
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(TIMEOUT)
            sock.bind(('192.168.1.50', src_port))
            start_time = time.perf_counter()
            try:
                sock.sendto(f"data from client{i}".encode(), (VIP, PORT))
                #data, addr = sock.recvfrom(1024)
                end_time = time.perf_counter()
                
                latency = (end_time - start_time) * 1000
                latencies.append(latency)
                results.append(True)

                if (i+1) % 80 == 0:
                    print(f"Sent packet {i+1} from source port {src_port}")
                    #print(f"packet {i}: success from {addr} in {latency:.2f}ms")
            except socket.timeout:
                lost_packets+=1
                results.append(False)

            time.sleep(0.02)


    print("Stress test completed.")

    if latencies:
        avg = statistics.mean(latencies)
        throughput = len(latencies) / (NUM_REQUESTS * 0.02)
        success_rate = ((NUM_REQUESTS-lost_packets)/NUM_REQUESTS)*100

        print("---performance report---")
        print(f"succes rate: {success_rate:.2f}%")
        print(f"average latency: {avg:.2f}ms")
        print(f"packet loss: {lost_packets}")
        print(f"throughput: {throughput}")
    else:
        print("test failed, no packets(ACK) returned")

    
    #save_performance_test(1, (lost_packets/NUM_REQUESTS)*100, avg, throughput)

def checksum(data):
    # Compute the checksum of the given data
    if len(data) % 2:
        data += b'\x00'
    s = sum(struct.unpack('!%dH' % (len(data) // 2), data))
    s = (s >> 16) + (s & 0xffff)
    s += s >> 16
    return ~s & 0xffff

def build_packet(src_ip, dst_ip, src_port, dst_port, seq_num, ack_num=0, flags=0x02):
    #ip header
    ip_ihl = 5
    ip_ver = 4
    ip_tos = 0
    ip_tot_len = 20 + 20
    ip_id = random.randint(0, 65535)
    ip_frag_off = 0
    ip_ttl = 64
    ip_proto = socket.IPPROTO_TCP
    ip_check = 0
    ip_saddr = socket.inet_aton(src_ip)
    ip_daddr = socket.inet_aton(dst_ip)

    ip_header = struct.pack('!BBHHHBBH4s4s', (ip_ver << 4) + ip_ihl, ip_tos, ip_tot_len, ip_id, ip_frag_off, ip_ttl, ip_proto, ip_check, ip_saddr, ip_daddr)
    ip_check = checksum(ip_header)
    ip_header = struct.pack('!BBHHHBBH4s4s', (ip_ver << 4) + ip_ihl, ip_tos, ip_tot_len, ip_id, ip_frag_off, ip_ttl, ip_proto, ip_check, ip_saddr, ip_daddr)

    #tcp header
    tcp_data_offset = 5
    tcp_flags = flags
    tcp_window = 5840
    tcp_check = 0
    tcp_urg_ptr = 0

    tcp_header = struct.pack('!HHLLBBHHH', src_port, dst_port, seq_num, ack_num, (tcp_data_offset << 4), tcp_flags, tcp_window, tcp_check, tcp_urg_ptr)

    #pseudo header for checksum
    placeholder = 0
    protocol = socket.IPPROTO_TCP
    tcp_length = len(tcp_header)

    psh = struct.pack('!4s4sBBH', ip_saddr, ip_daddr, placeholder, protocol, tcp_length)
    tcp_check = checksum(psh + tcp_header)

    tcp_header = struct.pack('!HHLLBBH', src_port, dst_port, seq_num, ack_num, (tcp_data_offset << 4), tcp_flags, tcp_window) + struct.pack('H', tcp_check) + struct.pack('!H', tcp_urg_ptr)

    return ip_header + tcp_header


def tcp_stress_test():
    SRC_IP = "192.168.1.50"
    print("Starting TCP SYN check")

    sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_TCP)
    sock.setsockopt(socket.IPPROTO_IP, socket.IP_HDRINCL, 1)

    recv_sock = socket.socket(socket.AF_INET, socket.SOCK_RAW, socket.IPPROTO_TCP)

    for i in range(5):
        src_port = 20000 + i
        seq_num = random.randint(0, 100000)

        packet = build_packet(SRC_IP, VIP, src_port, 80, seq_num, flags=0x02)  # SYN flag
        sock.sendto(packet, (VIP, 0))
        print(f"[CLIENT] sent SYN from port {src_port}")

        start = time.time()

        while time.time() - start < 2:
            data, _ = recv_sock.recvfrom(65535)

            ip_header = data[:20]
            iph = struct.unpack('!BBHHHBBH4s4s', ip_header)

            src_ip = socket.inet_ntoa(iph[8])
            dst_ip = socket.inet_ntoa(iph[9])

            tcp_header = data[20:40]
            tcph = struct.unpack('!HHLLBBHHH', tcp_header)

            src_p = tcph[0]
            dst_p = tcph[1]
            flags = tcph[5]

            syn = flags & 0x02
            ack = flags & 0x10

            if dst_ip == SRC_IP and dst_p == src_port:
                print(f"[CLIENT] received packet from {src_ip}:{src_p} with flags {flags:02x}")

                if syn and ack:
                    print(f"[CLIENT] SYN-ACK received! handshake successful with {src_ip}:{src_port}")
                    #send ACK
                    ack_packet = build_packet(SRC_IP, VIP, src_port, 80, seq_num + 1, tcph[2]+1, flags=0x10)  # ACK flag
                    sock.sendto(ack_packet, (VIP, 0))
                    print(f"[CLIENT] sent ACK from port {src_port}")
                    break

                elif syn:
                    print(f"[CLIENT] received SYN from {src_ip}:{src_port} (unexpected)")
                elif ack:
                    print(f"[CLIENT] received ACK from {src_ip}:{src_port} (unexpected)")


if __name__ == "__main__":
    #stress_test()
    tcp_stress_test()