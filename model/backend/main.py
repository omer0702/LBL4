from fastapi import FastAPI
import psycopg2

app= FastAPI()

def get_db_connection():
    try:
        connection = psycopg2.connect(
            dbaname = "lbl4_db",
            user = "lbl_user",
            password = '0702',
            host = "loclhost",
            port = "5432"
        )
        return connection
    except Exception as e:
        print("db connection error: ", e)
        return None

@app.get("/")
def root():
    return {"message":"backend is running"}


@app.get("/tset_db")
def test_db():
    connection = get_db_connection()
    if connection:
        cursor = connection.cursor()
        cursor.execute("SELECT version();")
        version = cursor.fetchone()
        cursor.close()
        connection.close()
        return {"db_connection": "OK", "version": version}
    else:
        return {"db_connection": "FAILED"}
