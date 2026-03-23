import socket
import time
import statistics 

#from model.backend.db.queries import save_performance_test

VIP = "10.0.0.100"
PORT = 9000
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
                data, addr = sock.recvfrom(1024)
                end_time = time.perf_counter()
                
                latency = (end_time - start_time) * 1000
                latencies.append(latency)
                results.append(True)

                if (i+1) % 80 == 0:
                    print(f"Sent packet {i+1} from source port {src_port}")
                    print(f"packet {i}: success from {addr} in {latency:.2f}ms")
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


if __name__ == "__main__":
    stress_test()
