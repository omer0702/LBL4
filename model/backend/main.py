from fastapi import FastAPI, APIRouter, HTTPException
from db.connection import test_connection, get_test_data_from_db
from fastapi.middleware.cors import CORSMiddleware
import threading
from stats_service import serve_grpc
from contextlib import asynccontextmanager
from typing import List
import schemas
from db import queries
import subprocess
import os

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

router = APIRouter(prefix="/api/admin")
app.include_router(router)


PROJECT_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "../../../"))
SCRIPTS_DIR = os.path.join(PROJECT_ROOT, "controller/user_ebpf/protocol_test/src")
CLIENT_BIN = os.path.join(PROJECT_ROOT, "build/debug/controller/user_ebpf/protocol_test/protocol_client")


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

@app.get("/api/services/{service_id}/metrics")
async def get_metrics(service_id: int):
    return queries.get_service_history_multi(service_id)

@app.get("/api/services/{service_id}/backends")
async def get_backends(service_id: int):
    return queries.get_service_backends_detailed(service_id)

@app.get("/api/services/{service_id}/performance")
async def get_performance(service_id: int):
    data = queries.get_lastest_performance(service_id)
    if not data:
        return {"packet_loss": 0, "avg_latency": 0}
    return {"packet_loss": data[0], "avg_latency": data[1]}

    
@app.get("/api/live-traffic")
async def get_live_traffic():
    """שליפת המידע על הסשנים החיים מה-DB"""
    return queries.get_sessions()

#endpoints for simulator:
@router.post("/deploy-cluster")
async def deploy_cluster():
    """מריץ את run_backends.sh להקמת 6 שרתים"""
    try:
        # הרצה של ה-Bash Script
        process = subprocess.Popen(
            ["sudo", "./run_backends.sh"],
            cwd=SCRIPTS_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stdout, stderr = process.communicate()
        if process.returncode != 0:
            raise Exception(stderr)
        return {"status": "success", "message": "Cluster deployed successfully", "output": stdout}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/cleanup")
async def cleanup_system():
    """מריץ את cleanup.sh לניקוי המערכת"""
    try:
        process = subprocess.Popen(
            ["sudo", "./cleanup.sh"],
            cwd=SCRIPTS_DIR,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )
        stdout, stderr = process.communicate()
        return {"status": "success", "message": "System cleaned", "output": stdout}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/add-backend/{ip_suffix}/{logical_id}")
async def add_single_backend(ip_suffix: int, logical_id: int):
    """מריץ מופע בודד של protocol_client"""
    try:
        ip = f"127.0.0.{ip_suffix}"
        # הרצה ב-Background כדי לא לחסום את ה-API
        subprocess.Popen(
            ["sudo", CLIENT_BIN, ip, str(logical_id)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            start_new_session=True
        )
        return {"status": "success", "message": f"Backend {ip} starting..."}
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@router.post("/run-stress", response_model=List[schemas.StressTestResult])
async def run_stress():
    """מריץ את stress_test.py ומחלץ את התוצאות מהפלט"""
    try:
        process = subprocess.run(
            ["python3", "stress_test.py"],
            cwd=SCRIPTS_DIR,
            capture_output=True,
            text=True
        )
        
        output = process.stdout
        # לוגיקת חילוץ נתונים פשוטה מה-Print של הסקריפט שלך
        results = {
            "success_rate": 0,
            "avg_latency": 0,
            "packet_loss": 0,
            "throughput": 0
        }
        
        for line in output.split('\n'):
            if "succes rate:" in line:
                results["success_rate"] = float(line.split(":")[1].replace('%', '').strip())
            elif "average latency:" in line:
                results["avg_latency"] = float(line.split(":")[1].replace('ms', '').strip())
            elif "packet loss:" in line:
                results["packet_loss"] = int(line.split(":")[1].strip())
            elif "throughput:" in line:
                results["throughput"] = float(line.split(":")[1].strip())

        return results
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))


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