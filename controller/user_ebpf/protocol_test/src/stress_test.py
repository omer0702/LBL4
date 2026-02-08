import socket
import time

VIP = "10.0.0.100"
PORT = 9000
MESSAGE = "Hello, World!"
NUM_REQUESTS = 100

def stress_test():
    print(f"Starting stress test: sending {NUM_REQUESTS} UDP packets to {VIP}:{PORT}")
    for i in range(NUM_REQUESTS):
        src_port = 10000 + i
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.bind(('0.0.0.0', src_port))
            try:
                sock.sendto(f"data from client{i}".encode(), (VIP, PORT))
                #src_port = sock.getsockname()[1]
                if (i+1) % 20 == 0:
                    print(f"Sent packet {i+1} from source port {src_port}")
                time.sleep(0.05)
            except Exception as e:
                print(f"Error sending packet {i+1}: {e}")
    print("Stress test completed.")
    

if __name__ == "__main__":
    stress_test()
