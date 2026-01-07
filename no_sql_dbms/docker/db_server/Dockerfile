FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    make \
    libssl-dev \
    netcat-openbsd \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY include/ ./include/
COPY src/ ./src/
COPY parcer/ ./parcer/
COPY Makefile .

RUN cd src && \
    g++ -std=c++17 -O2 -I../include -I../parcer -pthread \
    db_server.cpp utils.cpp query_evaluator.cpp btree_index.cpp collection.cpp \
    -o ../db_server

RUN mkdir -p /data/databases

EXPOSE 8080

COPY <<'EOF' /start-db.sh
#!/bin/bash
set -e

echo "Starting NoSQL DB Server..."
echo "Port: 8080"
echo "Data directory: /data/databases"

./db_server 8080 /data/databases &

echo "Waiting for server to start listening..."
timeout 30 bash -c 'until nc -z localhost 8080; do sleep 1; echo "Waiting for port 8080..."; done'

echo "NoSQL DB Server is ready and listening on port 8080"
echo "DB Server listening on port 8080"
echo "Database directory: /data/databases"

wait
EOF

RUN chmod +x /start-db.sh

CMD ["/start-db.sh"]
