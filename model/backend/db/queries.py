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


def insert_event(event_type, severity, service_name, message, metadata_json=None):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""INSERT INTO events (event_type, severity, service_name, message, metadata)
                         VALUES (%s, %s, %s, %s, %s)""",
                        (event_type, severity, service_name, message, metadata_json))
    finally:
        conn.commit()
        release_connection(conn)


def get_services_overview():
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT s.service_id, s.name, s.vip, 
                       COUNT(b.backend_id) as total_backends,
                       COUNT(CASE WHEN b.is_active = TRUE THEN 1 END) as active_backends,
                       COALESCE(SUM(m.pps), 0) as total_pps
                FROM services s
                LEFT JOIN backends b ON s.service_id = b.service_id
                LEFT JOIN LATERAL (
                    SELECT pps FROM metrics_history 
                    WHERE backend_id = b.backend_id 
                    ORDER BY timestamp DESC LIMIT 1
                ) m ON TRUE
                GROUP BY s.service_id;
            """)
            rows = cur.fetchall()
            return [{
                "service_id": r[0], "name": r[1], "vip": r[2], 
                "total_backends": r[3], "active_backends": r[4], 
                "total_pps": r[5]
            } for r in rows]
    finally:
        release_connection(conn)


def get_latest_events(limit=20):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT event_id, timestamp, event_type, severity, service_name, message, metadata 
                FROM events ORDER BY timestamp DESC LIMIT %s;
            """, (limit,))
            rows = cur.fetchall()
            return [{"event_id": r[0], "timestamp": r[1], "event_type": r[2], 
                     "severity": r[3], "service_name": r[4], "message": r[5], "metadata": r[6]} for r in rows]
    finally:
        release_connection(conn)