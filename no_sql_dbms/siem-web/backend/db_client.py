import socket
import json
import time
from typing import Dict, List, Optional, Any

class DBSocketClient:

    def __init__(self, host: str = "127.0.0.1", port: int = 8080):
        self.host = host
        self.port = port
        self.timeout = 5
        self.socket = None

    def _connect(self) -> bool:
        try:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.socket.settimeout(self.timeout)
            self.socket.connect((self.host, self.port))
            print(f"[DB Client] Connected to {self.host}:{self.port}")
            return True
        except Exception as e:
            print(f"[DB Client] Connection error: {e}")
            return False

    def _disconnect(self):
        if self.socket:
            self.socket.close()
            self.socket = None

    def send_request(self, database: str, operation: str,
                     data: Optional[List] = None,
                     query: Optional[Dict] = None) -> Dict:

        if query is None:
            query = {}

        request = {
            "database": database,
            "operation": operation,
            "query": query
        }

        if data:
            request["data"] = data

        print(f"[DB Client] Sending {operation} to {database}")
        return self._send_json(request)

    def _send_json(self, data: Dict) -> Dict:
        if not self.socket and not self._connect():
            return {"status": "error", "message": "Failed to connect to DB"}

        try:
            json_str = json.dumps(data) + "\n"
            print(f"[DB Client] Sending JSON: {json_str[:100]}...")

            self.socket.sendall(json_str.encode('utf-8'))

            response = b""
            start_time = time.time()

            while time.time() - start_time < self.timeout:
                try:
                    chunk = self.socket.recv(4096)
                    if not chunk:
                        break
                    response += chunk
                    if b"\n" in response:
                        break
                except socket.timeout:
                    break

            if not response:
                self._disconnect()
                return {"status": "error", "message": "No response from DB"}

            response_str = response.decode('utf-8').strip()
            print(f"[DB Client] Received: {response_str[:200]}...")

            try:
                result = json.loads(response_str)
                return result
            except json.JSONDecodeError as e:
                return {"status": "error", "message": f"Invalid JSON response: {e}"}

        except Exception as e:
            self._disconnect()
            return {"status": "error", "message": f"Communication error: {str(e)}"}

    def get_security_events(self, query: Optional[Dict] = None, limit: int = 1000) -> List[Dict]:

        if query is None:
            query = {}

        print(f"[DB Client] Getting security events, query: {query}")

        response = self.send_request(
            database="security_events",
            operation="find",
            query=query
        )

        if response.get("status") == "success":
            events = response.get("data", [])
            print(f"[DB Client] Retrieved {len(events)} events")
            return events[:limit]
        else:
            error_msg = response.get('message', 'Unknown error')
            print(f"[DB Client] Error: {error_msg}")
            return []

    def test_connection(self) -> bool:
        try:
            response = self.send_request(
                database="security_events",
                operation="find",
                query={}
            )
            return "status" in response
        except Exception as e:
            print(f"[DB Client] Test connection failed: {e}")
            return False
