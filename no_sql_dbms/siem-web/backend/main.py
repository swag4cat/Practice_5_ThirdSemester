from fastapi import FastAPI, Depends, HTTPException, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse
from fastapi.staticfiles import StaticFiles
from pathlib import Path
import secrets
from datetime import datetime, timedelta
from typing import List, Dict, Any
import os

from db_client import DBSocketClient
from processors import process_real_events

BASE_DIR = Path(__file__).parent.parent
FRONTEND_DIR = BASE_DIR / "frontend"

app = FastAPI(
    title="SIEM Web Interface",
    version="1.0.0",
    description="Security Information and Event Management System",
    docs_url="/api/docs",
    redoc_url="/api/redoc"
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

app.mount("/static", StaticFiles(directory=str(FRONTEND_DIR)), name="static")

db_client = DBSocketClient("127.0.0.1", 8080)

USERS = {
    "admin": "admin123",
    "user": "password123",
    "siem": "security2025"
}

security = HTTPBasic()

def get_current_user(credentials: HTTPBasicCredentials = Depends(security)):

    username = credentials.username
    password = credentials.password

    if username in USERS and USERS[username] == password:
        return username

    raise HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Incorrect username or password",
        headers={"WWW-Authenticate": "Basic"},
    )

@app.get("/")
async def root():
    return FileResponse(str(FRONTEND_DIR / "index.html"))

@app.get("/dashboard")
async def dashboard():
    return FileResponse(str(FRONTEND_DIR / "dashboard.html"))

@app.get("/events")
async def events_page():
    return FileResponse(str(FRONTEND_DIR / "events.html"))

@app.get("/api/health")
async def health_check():
    db_connected = db_client.test_connection()
    return {
        "status": "healthy",
        "timestamp": "2025-12-21T00:00:00Z",
        "services": {
            "api": "running",
            "database": "connected" if db_connected else "disconnected",
            "authentication": "enabled"
        }
    }

@app.get("/api/dashboard/summary")
async def get_dashboard_summary(credentials: HTTPBasicCredentials = Depends(security)):
    user = get_current_user(credentials)

    try:
        events = db_client.get_security_events(limit=5000)

        if events:
            result = process_real_events(events)
            result["db_connected"] = True
            result["message"] = f"Using real data: {len(events)} events"
            return result
        else:
            return {
                "active_agents": [],
                "auth_logs": [],
                "events_today": 0,
                "critical_events": 0,
                "unique_hosts": 0,
                "events_by_type": {},
                "events_by_severity": {},
                "top_users": [],
                "top_processes": [],
                "db_connected": True,
                "message": "Database connected but no events found"
            }

    except Exception as e:
        print(f"[API Error] {e}")
        return {
            "active_agents": [],
            "auth_logs": [],
            "events_today": 0,
            "critical_events": 0,
            "unique_hosts": 0,
            "events_by_type": {},
            "events_by_severity": {},
            "top_users": [],
            "top_processes": [],
            "db_connected": False,
            "message": f"Database error: {str(e)[:100]}"
        }

@app.get("/api/events")
async def get_events(
    skip: int = 0,
    limit: int = 50,
    credentials: HTTPBasicCredentials = Depends(security)
):

    user = get_current_user(credentials)

    try:
        events = db_client.get_security_events(limit=1000)
        db_connected = True

        if not events:
            print(f"[Events API] No events found in database")
            return {
                "events": [],
                "total": 0,
                "page": 1,
                "page_size": limit,
                "pages": 1,
                "db_connected": db_connected,
                "message": "No events found in database"
            }

        total = len(events)
        paginated_events = events[skip:skip + limit]

        return {
            "events": paginated_events,
            "total": total,
            "page": skip // limit + 1 if limit > 0 else 1,
            "page_size": limit,
            "pages": (total + limit - 1) // limit if limit > 0 else 1,
            "db_connected": db_connected,
            "message": f"Found {total} events"
        }

    except Exception as e:
        print(f"[Events API Error] {e}")
        return {
            "events": [],
            "total": 0,
            "page": 1,
            "page_size": limit,
            "pages": 1,
            "db_connected": False,
            "message": f"Database error: {str(e)[:100]}"
        }

@app.get("/api/events/stats/timeline")
async def get_events_timeline(
    hours: int = 24,
    credentials: HTTPBasicCredentials = Depends(security)
):

    user = get_current_user(credentials)

    try:
        events = db_client.get_security_events(limit=5000)

        print(f"[Timeline API] Retrieved {len(events)} events from DB")

        timeline = {f"{i:02d}:00": 0 for i in range(24)}

        if not events:
            print(f"[Timeline API] No events, returning empty timeline")
            return timeline

        from datetime import timezone
        now = datetime.now(timezone.utc)
        print(f"[Timeline API] Current UTC time (aware): {now}")
        print(f"[Timeline API] Events to process: {len(events)}")

        event_count = 0
        added_count = 0

        for event in events:
            timestamp_str = event.get('timestamp', '')
            if timestamp_str:
                try:
                    if timestamp_str.endswith('Z'):
                        timestamp_str = timestamp_str[:-1] + '+00:00'
                    event_time = datetime.fromisoformat(timestamp_str)

                    if event_time.tzinfo is None:
                        event_time = event_time.replace(tzinfo=timezone.utc)

                    if event_count < 3:
                        print(f"[Timeline API] Event {event_count}: time={event_time}, hour={event_time.hour}")
                        event_count += 1

                    hours_diff = (now - event_time).total_seconds() / 3600
                    if hours_diff <= 24:
                        hour_key = f"{event_time.hour:02d}:00"
                        timeline[hour_key] = timeline.get(hour_key, 0) + 1
                        added_count += 1
                        if added_count <= 3:
                            print(f"[Timeline API] Added event to hour {hour_key}, total in this hour: {timeline[hour_key]}")

                except Exception as e:
                    print(f"[Timeline API] Error parsing timestamp {timestamp_str}: {e}")
                    import traceback
                    traceback.print_exc()
                    continue

        print(f"[Timeline API] Total events added to timeline: {added_count}")
        print(f"[Timeline API] Final timeline data:")

        non_zero_hours = {hour: count for hour, count in timeline.items() if count > 0}
        if non_zero_hours:
            for hour, count in non_zero_hours.items():
                print(f"  {hour}: {count} events")
        else:
            print("  No events in any hour (all zeros)")

        return timeline

    except Exception as e:
        print(f"[Timeline API Error] {e}")
        import traceback
        traceback.print_exc()
        return {f"{i:02d}:00": 0 for i in range(24)}

@app.get("/api/events/stats/by-type")
async def get_events_by_type(credentials: HTTPBasicCredentials = Depends(security)):
    user = get_current_user(credentials)

    try:
        events = db_client.get_security_events(limit=5000)

        if events:
            type_counter = {}
            for event in events:
                event_type = event.get("event_type", "unknown")
                type_counter[event_type] = type_counter.get(event_type, 0) + 1
            return type_counter
        else:
            return {}

    except Exception as e:
        return {}

@app.get("/api/events/stats/by-severity")
async def get_events_by_severity(credentials: HTTPBasicCredentials = Depends(security)):
    user = get_current_user(credentials)

    try:
        events = db_client.get_security_events(limit=5000)

        if events:
            severity_counter = {}
            for event in events:
                severity = event.get("severity", "info")
                severity_counter[severity] = severity_counter.get(severity, 0) + 1
            return severity_counter
        else:
            return {}

    except Exception as e:
        return {}

@app.get("/api/agents")
async def get_agents(credentials: HTTPBasicCredentials = Depends(security)):
    user = get_current_user(credentials)

    try:
        events = db_client.get_security_events(limit=1000)

        if events:
            agents = {}
            for event in events:
                hostname = event.get("hostname", "unknown")
                timestamp = event.get("timestamp", "")

                if hostname not in agents:
                    agents[hostname] = {
                        "hostname": hostname,
                        "last_activity": timestamp,
                        "event_count": 0,
                        "last_event_type": event.get("event_type", ""),
                        "severity": event.get("severity", "info")
                    }

                if timestamp > agents[hostname].get("last_activity", ""):
                    agents[hostname]["last_activity"] = timestamp
                    agents[hostname]["last_event_type"] = event.get("event_type", "")
                    agents[hostname]["severity"] = event.get("severity", "info")

                agents[hostname]["event_count"] += 1

            return {
                "agents": list(agents.values()),
                "total": len(agents),
                "db_connected": True
            }
        else:
            return {}

    except Exception as e:
        return {
            "agents": [],
            "total": 0,
            "db_connected": False,
            "message": f"Error: {str(e)[:100]}"
        }


@app.get("/js/{filename}")
async def get_js(filename: str):
    js_path = FRONTEND_DIR / "js" / filename
    if js_path.exists():
        return FileResponse(str(js_path))
    raise HTTPException(status_code=404, detail="JavaScript file not found")

@app.get("/css/{filename}")
async def get_css(filename: str):
    css_path = FRONTEND_DIR / "css" / filename
    if css_path.exists():
        return FileResponse(str(css_path))
    raise HTTPException(status_code=404, detail="CSS file not found")

@app.get("/assets/{filename}")
async def get_asset(filename: str):
    asset_path = FRONTEND_DIR / "assets" / filename
    if asset_path.exists():
        return FileResponse(str(asset_path))
    raise HTTPException(status_code=404, detail="Asset file not found")

@app.get("/favicon.ico")
async def get_favicon():
    favicon_path = FRONTEND_DIR / "assets" / "favicon.svg"
    if favicon_path.exists():
        return FileResponse(str(favicon_path), media_type="image/svg+xml")
    raise HTTPException(status_code=404, detail="Favicon not found")


if __name__ == "__main__":
    import uvicorn

    print("=" * 30)
    print("     SIEM Web Interface")
    print("=" * 30)
    print(f"Frontend directory: {FRONTEND_DIR}")
    print(f"Database client: {db_client.host}:{db_client.port}")

    try:
        db_connected = db_client.test_connection()
        if db_connected:
            print("Database connection: SUCCESS")
        else:
            print("Database connection: FAILED")
    except Exception as e:
        print(f"Database connection error: {e}")

    print("Starting server on http://0.0.0.0:8000")

    print("=" * 30)
    print("     Press Ctrl+C to stop")
    print("=" * 30)

    uvicorn.run(
        app,
        host="0.0.0.0",
        port=8000,
        reload=False,
        log_level="info"
    )
