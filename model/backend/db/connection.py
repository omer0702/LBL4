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


def get_test_data_from_db():
    connection = create_connection()
    cursor = connection.cursor()
    cursor.execute("SELECT name FROM test_data") #fake table, just for checks
    results = [row[0] for row in cursor.fetchall()]
    cursor.close()
    connection.close()

    return results
    

#add with psql the real tables, only one time
#funcs: insert_service_data, update_service_data, get_service_details and more