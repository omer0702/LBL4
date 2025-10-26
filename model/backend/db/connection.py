import psycopg2
from psycopg2 import OperationalError



def create_connection():
    try:
        connection = psycopg2.connect(
            dbname = "lbl4_db",
            user = "lbl_user",
            password = '0702',
            host = "localhost",
            port = "5432"
        )
        return connection
    except Exception as e:
        print("db connection error: ", e)
        return None




def test_connection():
    connection = create_connection()
    if connection:
        cursor = connection.cursor()
        cursor.execute("SELECT version();")
        version = cursor.fetchone()
        cursor.close()
        connection.close()
        return {"db_connection": "OK", "version": version}
    else:
        return {"db_connection": "FAILED"}
