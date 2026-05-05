import grpc
from concurrent import futures
import internal_stats_pb2
import internal_stats_pb2_grpc
from db.queries import get_or_create_backend, insert_metrics, insert_event, save_live_sessions
from datetime import datetime
import ipaddress
import socket
import struct


class StatsCollectorServicer(internal_stats_pb2_grpc.StatsCollectorServicer):
    def ExportStats(self, request, context):
        try:
            timestamp = datetime.fromtimestamp(request.timestamp)

            for stat in request.stats:
                #print(f"Received stats for service: {stat.service_name} at {timestamp}")
                backend_id = get_or_create_backend(stat.service_name, str(ipaddress.IPv4Address(stat.ip)), stat.port, stat.logical_id)
                insert_metrics(backend_id, timestamp, stat.cpu_usage, stat.mem_usage, stat.active_requests, stat.pps, stat.bps, stat.total_packets)

            return internal_stats_pb2.StatsResponse(success=True, message="Stats collected successfully")
        except Exception as e:
            print(f"\nError processing stats: {e}\n")
            return internal_stats_pb2.StatsResponse(success=False, message=str(e))


    def LogEvent(self, request, context):
        try:
            severiry_map = {0: 'INFO', 1: 'WARNING', 2: 'ERROR'}
            severity_str = severiry_map.get(request.severity, 'INFO')

            insert_event(event_type=request.event_type, 
                         severity=severity_str, 
                         service_name=request.service_name, 
                         message=request.message, 
                         metadata_json=request.metadata if request.metadata_json else None)
            
            return internal_stats_pb2.LogResponse(success=True)
        except Exception as e:
            print(f"\nError logging event: {e}\n")
            return internal_stats_pb2.LogResponse(success=False)

    def UpdateLiveSessions(self, request, context):
        sessions_list = []
        for session in request.sessions:
            src_ip = socket.inet_ntoa(struct.pack('<L', session.src_ip))
            dst_ip = socket.inet_ntoa(struct.pack('<L', session.dst_ip))

            session_data = {
                'src_ip': src_ip,
                'dst_ip': dst_ip,
                'src_port': session.src_port,
                'dst_port': session.dst_port,
                'protocol': "TCP" if session.protocol == 6 else "UDP",
                'timestamp': session.timestamp
            }

            sessions_list.append(session_data)

        save_live_sessions(sessions_list)

        return internal_stats_pb2.Empty()

def serve_grpc():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    internal_stats_pb2_grpc.add_StatsCollectorServicer_to_server(StatsCollectorServicer(), server)
    server.add_insecure_port('[::]:50051')
    server.start()
    print("gRPC server started on port 50051")
    server.wait_for_termination()