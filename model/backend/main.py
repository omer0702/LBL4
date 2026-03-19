from fastapi import FastAPI
from db.connection import test_connection, get_test_data_from_db
from fastapi.middleware.cors import CORSMiddleware
import threading
from stats_service import serve_grpc
from contextlib import asynccontextmanager
from typing import List
import schemas
from db import queries

@asynccontextmanager
async def lifespan(app: FastAPI):
    #Start the gRPC server in a separate thread
    grpc_thread = threading.Thread(target=serve_grpc, daemon=True)
    grpc_thread.start()
    yield

app= FastAPI(lifespan=lifespan)

app.add_middleware(#enable to react(3000) access to backend(8000)
    CORSMiddleware,
    allow_origins=["*"],#open for everyone, maybe enable only for 3000
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

#Endpoints:
@app.get("/api/overview", response_model=List[schemas.ServiceOverviewSchema])
async def get_overview():
    """שליפת תמונת מצב כללית עבור ה-Dashboard"""
    return queries.get_services_overview()

@app.get("/api/events", response_model=List[schemas.EventSchema])
async def get_events(limit: int = 20):
    """שליפת האירועים האחרונים מה-Logger"""
    return queries.get_latest_events(limit)

@app.get("/api/health")
async def health_check():
    """בדיקת תקינות המערכת (Backend + DB)"""
    # כאן אפשר להוסיף בעתיד בדיקה אם ה-Controller שלח Heartbeat לאחרונה
    return {
        "status": "online",
        "engine": "FastAPI",
        "db_connected": True,
        "controller_connected": True # נשפר את זה כשנוסיף Heartbeat
    }

# Endpoint לשליפת נתונים מפורטים על שירות ספציפי (לשלב 4)
@app.get("/api/services/{service_id}/metrics")
async def get_service_metrics(service_id: int):
    pass


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