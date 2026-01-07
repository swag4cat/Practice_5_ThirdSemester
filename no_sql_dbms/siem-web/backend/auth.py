import os
from fastapi import Depends, HTTPException, status
from fastapi.security import HTTPBasic, HTTPBasicCredentials
import secrets
from dotenv import load_dotenv

load_dotenv()

security = HTTPBasic()

def get_current_user(credentials: HTTPBasicCredentials = Depends(security)):

    users_str = os.getenv("SIEM_USERS", "")

    if not users_str:
        print("[Auth] WARNING: SIEM_USERS not set in .env")
    else:
        USERS = {}
        for user_entry in users_str.split(','):
            if ':' in user_entry:
                username, password = user_entry.strip().split(':', 1)
                USERS[username] = password

        print(f"[Auth] Loaded {len(USERS)} users from .env")

    username = credentials.username
    password = credentials.password

    if username in USERS and USERS[username] == password:
        return username

    raise HTTPException(
        status_code=status.HTTP_401_UNAUTHORIZED,
        detail="Incorrect username or password",
        headers={"WWW-Authenticate": "Basic"},
    )
