# Docker

**Практика 5**: Контейнеризация и оркестрация распределенной SIEM-системы

**Цель**: Получить практические навыки в контейнеризации приложений, работе с Docker, Docker Compose, создании многокомпонентных систем и автоматизации развертывания.
**Задание**: Разработать Docker-образы для всех компонентов SIEM-системы (СУБД, веб-интерфейс, веб-сервер) и создать сценарии оркестрации с помощью Docker Compose для их совместного запуска и взаимодействия.

---

## Возможности

### Полная контейнеризация всех компонентов

- NoSQL СУБД (C++ сервер) - контейнеризован
- Backend API (Python FastAPI) - контейнеризован
- Frontend (Nginx с SPA) - контейнеризован
- SIEM Agent - готов к запуску в Docker-среде

### Автоматическая оркестрация

- Запуск всех сервисов одной командой
- Автоматическое подключение к сети
- Healthcheck для мониторинга состояния
- Сохранение данных при перезапуске

### Готовая к production среда

- Изолированные контейнеры
- Правильные volume-монтирования
- Логирование через docker logs

---

## Архитектура проекта

```text
no_sql_dbms/
│
├── docker/                             # Docker-конфигурации
│   ├── db_server/
│   │   └── Dockerfile                  # Образ NoSQL СУБД
│   ├── backend/
│   │   └── Dockerfile                  # Образ FastAPI Backend
│   └── frontend/
│       └── Dockerfile                  # Образ Nginx Frontend
│
├── docker-compose.yml                  # Оркестрация всех сервисов
│
├── include/                            # Заголовочные файлы СУБД
├── src/                                # Исходный код СУБД
├── parcer/                             # JSON-парсер
├── siem_agent/                         # SIEM Agent
├── siem-web/                           # Веб-интерфейс
├── systemd/                            # Демонизация агента
├── Makefile
└── databases/                          # Хранилище данных (volume)
```

---

## Docker-образы

1. NoSQL Database Server (nosql-db:latest)
- Основа: Ubuntu 22.04
- Размер: ~309MB
- Порт: 8080 (внутри сети), 27017 (снаружи)
- Особенности: собственная C++ NoSQL СУБД с поддержкой JSON документов

2. SIEM Backend API (siem-backend:latest)
- Основа: Python 3.11-slim
- Размер: ~171MB
- Порт: 8000
- Особенности: FastAPI с аутентификацией, аналитикой и интеграцией с СУБД

3. SIEM Frontend (siem-frontend:latest)
- Основа: Nginx Alpine
- Размер: ~54MB
- Порт: 80
- Особенности: Reverse proxy для backend, отдача статики SPA

---

## Быстрый старт

1. Клонирование и сборка
```bash
# Клонировать репозиторий
git clone <repository-url>
cd no_sql_dbms

# Собрать все Docker-образы
docker-compose build
```

2. Запуск всей системы
```bash
# Запуск всех сервисов
docker-compose up -d

# Проверка состояния
docker-compose ps
```

3. Доступ к системе
```bash
# Веб-интерфейс (основной)
http://localhost:8000

# Прямой доступ к NoSQL СУБД
echo '{"database":"test","operation":"find","query":{}}' | nc localhost 27017

# Проверка здоровья API
curl http://localhost:8000/api/health
```

---

## Docker Compose сервисы

1. nosql-db
- Порт: 27017
- Назначение: NoSQL СУБД
- Healthcheck: проверка подключения

2. backend
- Порт: 8000
- Назначение: FastAPI API
- Healthcheck: /api/health эндпоинт

3. nginx
- Порт: 8080
- Назначение: Reverse proxy
- Healthcheck: статическая страница

---

## Команды управления

```bash
# Запуск всей системы
docker-compose up -d

# Остановка всей системы
docker-compose down

# Перезапуск
docker-compose restart

# Просмотр логов
docker-compose logs -f
docker-compose logs backend     # логи только backend
docker-compose logs nosql-db    # логи только БД

# Проверка состояния
docker-compose ps

# Пересборка образов
docker-compose build

# Полная очистка
docker-compose down -v
docker system prune -f
```

---

## Тестирование работы системы

```bash
# 1. Проверка NoSQL СУБД
echo '{"database":"security_events","operation":"find","query":{}}' | nc localhost 27017

# 2. Проверка Backend API
curl http://localhost:8000/api/health

# 3. Тест авторизации
curl -u admin:admin123 http://localhost:8000/api/dashboard/summary

# 4. Тест сохранения данных
echo '{"database":"security_events","operation":"insert","data":[{"test":"docker"}]}' | nc localhost 27017
docker-compose restart
echo '{"database":"security_events","operation":"find","query":{}}' | nc localhost 27017

# 5. Проверка сети между сервисами
docker network inspect no_sql_dbms_siem-network
```

## Volumes и данные

Система использует Docker volumes для сохранения данных:

```bash
# Просмотр volumes
docker volume ls | grep no_sql_dbms

# Данные NoSQL СУБД
no_sql_dbms_nosql_data -> /data/databases

# Данные Backend
no_sql_dbms_backend_data -> /app/siem_web/data

# Логи Nginx
no_sql_dbms_nginx_logs -> /var/log/nginx
```

---

## Интеграция с SIEM Agent

SIEM Agent может быть запущен в той же Docker-сети:

```bash
./siem_agent_bin --config siem_agent/configs/agent_config.json
```

