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


def get_service_history_by_time(service_id, minutes=10):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT 
                    date_trunc('second', m.timestamp) as ts,
                    SUM(m.pps) as total_pps,
                    SUM(m.bps) as total_bps,
                    AVG((m.cpu_usage + m.mem_usage) / 2.0) as avg_score
                FROM metrics_history m
                JOIN backends b ON m.backend_id = b.backend_id
                WHERE b.service_id = %s AND m.timestamp > NOW() - INTERVAL '%s minutes'
                GROUP BY ts
                ORDER BY ts ASC;
            """, (service_id, minutes))
            rows = cur.fetchall()
            return [{"timestamp": r[0].isoformat(), "pps": float(r[1]), "bps": float(r[2]), "score": float(r[3])} for r in rows]
    finally:
        release_connection(conn)

def get_service_history(service_id, limit=50):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT ts, total_pps, total_bps, avg_score FROM (
                    SELECT 
                        date_trunc('second', m.timestamp) as ts,
                        SUM(m.pps) as total_pps,
                        SUM(m.bps) as total_bps,
                        AVG((m.cpu_usage + m.mem_usage) / 2.0) as avg_score,
                    FROM metrics_history m
                    JOIN backends b ON m.backend_id = b.backend_id
                    WHERE b.service_id = %s
                    GROUP BY ts
                    ORDER BY ts DESC
                    LIMIT %s
                ) sub
                ORDER BY ts ASC;
            """, (service_id, limit))
            rows = cur.fetchall()
            return [{"timestamp": r[0].isoformat(), "pps": float(r[1]), "bps": float(r[2]), "score": float(r[3])} for r in rows]
    finally:
        release_connection(conn)


def get_service_backends_detailed(service_id):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            # שליפת הנתונים הכי עדכניים עבור כל שרת בשירות
            cur.execute("""
                SELECT DISTINCT ON (b.backend_id)
                    b.backend_id, b.ip, b.port, b.is_active, b.logical_id,
                    m.cpu_usage, m.mem_usage, m.active_requests, m.pps, m.total_packets
                FROM backends b
                LEFT JOIN metrics_history m ON b.backend_id = m.backend_id
                WHERE b.service_id = %s
                ORDER BY b.backend_id, m.timestamp DESC;
            """, (service_id,))
            rows = cur.fetchall()
            return [{
                "id": r[0], "ip": r[1], "port": r[2], "active": r[3], "logical_id": r[4],
                "cpu": r[5] or 0, "mem": r[6] or 0, "active_req": r[7] or 0, 
                "pps": r[8] or 0, "total_packets": r[9] or 0,
                "score": 100 - ((r[5] or 0) + (r[6] or 0)) / 2 # חישוב ציון זמני (הפוך לעומס)
            } for r in rows]
    finally:
        release_connection(conn)


def get_service_history_multi(service_id, limit=50):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            # שליפת 50 דגימות אחרונות לכל שרת בשירות
            cur.execute("""
            SELECT ts as timestamp, logical_id, SUM(pps) as pps
            FROM(
                SELECT date_trunc('second',m.timestamp) as ts, b.logical_id, m.pps,
                    ROW_NUMBER() OVER (PARTITION BY b.backend_id, date_trunc('second',m.timestamp) ORDER BY m.timestamp DESC) as rn
                FROM metrics_history m
                JOIN backends b ON m.backend_id = b.backend_id
                WHERE b.service_id = %s
            ) t
            WHERE rn <= %s
            GROUP BY ts, logical_id
            ORDER BY ts ASC;
            """, (service_id, limit))
            rows = cur.fetchall()
            
            # עיבוד הנתונים למבנה ש-Recharts אוהב: { timestamp: T, server1: PPS, server2: PPS, total: PPS }
            data_map = {}
            for ts, log_id, pps in rows:
                ts_str = ts.isoformat()
                if ts_str not in data_map:
                    data_map[ts_str] = {"timestamp": ts_str, "total": 0}
                
                key = f"server_{log_id}"
                data_map[ts_str][key] = float(pps)
                data_map[ts_str]["total"] += float(pps)
            
            return sorted(list(data_map.values()), key=lambda x: x["timestamp"])
    finally:
        release_connection(conn)


def save_performance_test(service_id, loss, latency, throughput):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                INSERT INTO service_performance (service_id, packet_loss_pct, avg_latency_ms, throughput_pps)
                VALUES (%s, %s, %s, %s)
            """, (service_id, loss, latency, throughput))
            conn.commit(conn)
    finally:
        release_connection()

def get_lastest_performance(service_id):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT packet_loss_pct, avg_latency_ms
                        FROM service_performance
                        WHERE service_id = %s
                        ORDER BY timestamp DESC
                        LIMIT 1
            """, (service_id,))
            return cur.fetchone()
    finally:
        release_connection(conn)


def save_live_sessions(sessions):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            for session in sessions:
                #insert or update new sessions:
                cur.execute("""
                    INSERT INTO live_sessions (src_ip, dst_ip, src_port, dst_port, protocol, last_seen)
                    VALUES (%s, %s, %s, %s, %s, %s)
                    ON CONFLICT (src_ip, dst_ip, src_port, dst_port, protocol)
                    DO UPDATE SET last_seen = EXCLUDED.last_seen
                """, (session['src_ip'], session['dst_ip'], session['src_port'], session['dst_port'], session['protocol'], datetime.fromtimestamp(session['timestamp'])))
            conn.commit()
    finally:
        release_connection(conn)

def get_sessions():
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT src_ip, dst_ip, src_port, dst_port, protocol, last_seen
                FROM live_sessions
                WHERE last_seen > NOW() - INTERVAL '60 minutes'
            """)
            rows = cur.fetchall()
            return [{
                "src_ip": r[0], "dst_ip": r[1], "src_port": r[2], 
                "dst_port": r[3], "protocol": r[4], "timestamp": r[5].isoformat()
            } for r in rows]
    finally:
        release_connection(conn)