from .connection import get_connection, release_connection
from datetime import datetime


def insert_service(name, vip):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""INSERT INTO services (name, vip)
                         VALUES (%s, %s) ON CONFLICT (name)
                         DO UPDATE SET vip = EXCLUDED.vip
                        RETURNING service_id;
                        """,
                        (name, vip))
            return cur.fetchone()[0]
    finally:
        conn.commit()
        release_connection(conn)

def get_or_create_backend(service_name, ip, port, logical_id):
    service_id = insert_service(service_name, "0.0.0.0")
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""INSERT INTO backends (service_id, ip, port, logical_id)
                         VALUES (%s, %s, %s, %s)
                        ON CONFLICT (service_id, ip, port) DO UPDATE SET logical_id = EXCLUDED.logical_id
                         RETURNING backend_id;""",
                        (service_id, ip, port, logical_id))
            return cur.fetchone()[0]
    finally:
        conn.commit()
        release_connection(conn)


def insert_metrics(backend_id, timestamp, cpu, mem, active_req, pps, bps, total_packets):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""INSERT INTO metrics_history (backend_id, timestamp, cpu_usage, mem_usage, active_requests, pps, bps, total_packets)
                         VALUES (%s, %s, %s, %s, %s, %s, %s, %s)""",
                        (backend_id, timestamp, cpu, mem, active_req, pps, bps, total_packets))
    finally:
        conn.commit()
        release_connection(conn)