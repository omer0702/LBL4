import grpc
from concurrent import futures
import internal_stats_pb2
import internal_stats_pb2_grpc
from db.queries import get_or_create_backend, insert_metrics
from datetime import datetime

class StatsCollectorServicer(internal_stats_pb2_grpc.StatsCollectorServicer):
    def ExportStats(self, request, context):
        try:
            timestamp = datetime.fromtimestamp(request.timestamp)

            for stat in request.stats:
                print(f"Received stats for service: {stat.service_name} at {timestamp}")
                backend_id = get_or_create_backend(stat.service_name, stat.ip, 80, 555)
                insert_metrics(backend_id, timestamp, stat.cpu_usage, stat.mem_usage, stat.active_requests, stat.pps, stat.bps, stat.total_packets)

            return internal_stats_pb2.StatsResponse(success=True, message="Stats collected successfully")
        except Exception as e:
            print(f"\nError processing stats: {e}\n")
            return internal_stats_pb2.StatsResponse(success=False, message=str(e))


def serve_grpc():
    server = grpc.server(futures.ThreadPoolExecutor(max_workers=10))
    internal_stats_pb2_grpc.add_StatsCollectorServicer_to_server(StatsCollectorServicer(), server)
    server.add_insecure_port('[::]:50051')
    server.start()
    print("gRPC server started on port 50051")
    server.wait_for_termination()