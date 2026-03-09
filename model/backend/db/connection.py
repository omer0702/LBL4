import psycopg2
from psycopg2 import pool
import os


DB_CONFIG = {
    "dbname": "lbl4_db",
    "user": "lbl_user",
    "password": '0702',
    "host": "localhost",
    "port": "5432"
}

try:
    connection_pool = psycopg2.pool.SimpleConnectionPool(1, 10, **DB_CONFIG)
    if connection_pool:
        print("Connection pool created successfully")
except Exception as e:
    print(f"Error creating connection pool: {e}")

def get_connection():
    return connection_pool.getconn()

def release_connection(connection):
    connection_pool.putconn(connection)


def test_connection():
    connection = get_connection()
    if connection:
        cursor = connection.cursor()
        cursor.execute("SELECT version();")
        version = cursor.fetchone()
        cursor.close()
        release_connection(connection)
        return {"db_connection": "OK", "version": version}
    else:
        return {"db_connection": "FAILED"}


def get_test_data_from_db():
    connection = get_connection()
    cursor = connection.cursor()
    cursor.execute("SELECT name FROM test_data") #fake table, just for checks
    results = [row[0] for row in cursor.fetchall()]
    cursor.close()
    release_connection(connection)

    return results
    

#add with psql the real tables, only one time
#funcs: insert_service_data, update_service_data, get_service_details and more