# docker/frontend/Dockerfile (рабочий вариант)
FROM nginx:alpine

# 1. Копируем статические файлы фронтенда
COPY siem-web/frontend/ /usr/share/nginx/html/

# 2. Установим envsubst для подстановки переменных
RUN apk add --no-cache gettext

# 3. Создаем шаблон конфигурации
RUN cat > /etc/nginx/nginx.conf.template << 'EOF'
user nginx;
worker_processes auto;
error_log /var/log/nginx/error.log warn;
pid /var/run/nginx.pid;

events {
    worker_connections 1024;
}

http {
    include /etc/nginx/mime.types;
    default_type application/octet-stream;

    sendfile on;
    tcp_nopush on;
    tcp_nodelay on;
    keepalive_timeout 65;
    types_hash_max_size 2048;

    access_log /var/log/nginx/access.log;

    gzip on;
    gzip_vary on;
    gzip_min_length 1024;
    gzip_types text/plain text/css text/xml text/javascript
               application/json application/javascript application/xml+rss
               application/atom+xml image/svg+xml;

    # Конфигурация сервера SIEM
    server {
        listen 80;
        server_name localhost;

        root /usr/share/nginx/html;
        index index.html;

        location / {
            try_files $uri $uri/ /index.html;
        }

        # API запросы проксируем к backend
        location /api/ {
            proxy_pass http://${BACKEND_HOST}:${BACKEND_PORT};
            proxy_set_header Host $host;
            proxy_set_header X-Real-IP $remote_addr;
            proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
            proxy_set_header X-Forwarded-Proto $scheme;

            proxy_connect_timeout 60s;
            proxy_send_timeout 60s;
            proxy_read_timeout 60s;

            proxy_http_version 1.1;
            proxy_set_header Upgrade $http_upgrade;
            proxy_set_header Connection "upgrade";
        }

        # Статические файлы
        location /static/ {
            alias /usr/share/nginx/html/;
            expires 1y;
            add_header Cache-Control "public, immutable";
        }

        location /css/ {
            alias /usr/share/nginx/html/css/;
            expires 1y;
            add_header Cache-Control "public, immutable";
        }

        location /js/ {
            alias /usr/share/nginx/html/js/;
            expires 1y;
            add_header Cache-Control "public, immutable";
        }

        location /assets/ {
            alias /usr/share/nginx/html/assets/;
            expires 1y;
            add_header Cache-Control "public, immutable";
        }

        location /health {
            access_log off;
            return 200 "nginx: ok\n";
            add_header Content-Type text/plain;
        }

        error_page 404 /index.html;
        error_page 500 502 503 504 /50x.html;
        location = /50x.html {
            root /usr/share/nginx/html;
        }
    }
}
EOF

# 4. Создаем скрипт запуска
RUN cat > /docker-entrypoint.sh << 'EOF'
#!/bin/sh
set -e

# Подставляем переменные окружения в шаблон
envsubst '${BACKEND_HOST} ${BACKEND_PORT}' < /etc/nginx/nginx.conf.template > /etc/nginx/nginx.conf

# Проверяем конфигурацию
echo "=== Проверка конфигурации nginx ==="
echo "BACKEND_HOST: ${BACKEND_HOST}"
echo "BACKEND_PORT: ${BACKEND_PORT}"
nginx -t

echo "=== Запуск nginx ==="
exec nginx -g "daemon off;"
EOF

RUN chmod +x /docker-entrypoint.sh

EXPOSE 80

ENTRYPOINT ["/docker-entrypoint.sh"]
