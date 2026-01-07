FROM python:3.11-slim

WORKDIR /app

RUN apt-get update && apt-get install -y netcat-openbsd && rm -rf /var/lib/apt/lists/*

COPY siem-web/requirements.txt .

RUN pip install --no-cache-dir -r requirements.txt

RUN mkdir -p /app/siem_web

COPY siem-web/backend/ /app/siem_web/backend/
COPY siem-web/frontend/ /app/siem_web/frontend/

RUN echo '' > /app/siem_web/__init__.py && \
    echo '' > /app/siem_web/backend/__init__.py

RUN cd /app/siem_web/backend && \
    sed -i 's/from db_client import/from .db_client import/' main.py && \
    sed -i 's/from processors import/from .processors import/' main.py

RUN ln -sf /app/siem_web/backend /app/backend

EXPOSE 8000

WORKDIR /app/siem_web

COPY <<'EOF' /wait-for-db.sh
#!/bin/bash
set -e

echo "Waiting for database nosql-db:8080"
timeout 60 bash -c '
  until nc -z nosql-db 8080; do
    sleep 2
    echo "Database not ready yet..."
  done
'

echo "Database is ready"
echo "Starting backend"
exec uvicorn backend.main:app --host 0.0.0.0 --port 8000
EOF

RUN chmod +x /wait-for-db.sh

CMD ["/wait-for-db.sh"]
