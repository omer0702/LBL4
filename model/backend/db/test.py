# a simple script that try queries.py:
from .connection import get_connection, release_connection


# a tset function that insert row to test_data table:
def test_db(service_id, ip, port, logical_id):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("INSERT INTO backends (service_id, ip ,port, logical_id) VALUES (%s, %s, %s, %s)", (service_id, ip, port, logical_id))
    finally:
        if conn:
            conn.commit()
        release_connection(conn)

test_db(111, "192.168.1.1", 8080, 222)