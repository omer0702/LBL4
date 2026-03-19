from pydantic import BaseModel
from typing import List, Optional
from datetime import datetime

class BackendStatSchema(BaseModel):
    backend_id: int
    logical_id: int
    ip: str
    port: int
    service_name: str
    pps: float
    bps: float
    cpu_usage: int
    mem_usage: int
    active_requests: int
    total_packets: int
    last_update: datetime

class ServiceOverviewSchema(BaseModel):
    service_id: int
    name: str
    vip: str
    total_backends: int
    active_backends: int
    total_pps: float
   

class EventSchema(BaseModel):
    event_id: int
    timestamp: datetime
    event_type: str
    severity: str
    service_name: Optional[str]
    message: str
    metadata: Optional[dict]