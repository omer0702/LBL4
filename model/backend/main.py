from fastapi import FastAPI
from db.connection import test_connection, get_test_data_from_db
from fastapi.middleware.cors import CORSMiddleware


app= FastAPI()

app.add_middleware(#enable to react(3000) access to backend(8000)
    CORSMiddleware,
    allow_origins=["*"],#open for everyone, maybe enable only for 3000
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

#initial checks
@app.get("/")
def root():
    return {"message":"backend is running"}

@app.get("/api/hello")
def hello():
    return {"message":"hello from fastapi backend"}


@app.get("/test_db")
def test_db():
    return test_connection()


#check data from db to gui
@app.get("/api/test_data_from_db")
def test_data_from_db():
    return {"services": get_test_data_from_db()}