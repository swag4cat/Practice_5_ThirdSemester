CXX = g++
CXXFLAGS = -std=c++17 -O2 -I./include -I./parcer -pthread

# Директории
SRCDIR = src
SIEMDIR = siem_agent

# Исходники клиента СУБД
CLIENT_SOURCES = $(SRCDIR)/db_client.cpp $(SRCDIR)/utils.cpp

# Исходники сервера СУБД
SERVER_SOURCES = $(SRCDIR)/db_server.cpp $(SRCDIR)/utils.cpp \
				 $(SRCDIR)/query_evaluator.cpp $(SRCDIR)/btree_index.cpp \
				 $(SRCDIR)/collection.cpp

# Исходники SIEM-агента
SIEM_SOURCES = $(SIEMDIR)/src/agent.cpp $(SIEMDIR)/src/config.cpp \
			   $(SIEMDIR)/src/db_sender.cpp $(SIEMDIR)/src/event_buffer.cpp \
			   $(SIEMDIR)/src/event.cpp $(SIEMDIR)/src/log_collector.cpp \
			   $(SIEMDIR)/src/position_manager.cpp $(SIEMDIR)/src/main.cpp

# Цели
CLIENT_TARGET = db_client
SERVER_TARGET = db_server
SIEM_TARGET = siem_agent_bin

all: $(CLIENT_TARGET) $(SERVER_TARGET) $(SIEM_TARGET)

# Сборка клиента СУБД
$(CLIENT_TARGET): $(CLIENT_SOURCES)
	@echo "Building DB Client..."
	$(CXX) $(CXXFLAGS) -o $(CLIENT_TARGET) $(CLIENT_SOURCES)

# Сборка сервера СУБД
$(SERVER_TARGET): $(SERVER_SOURCES)
	@echo "Building DB Server..."
	$(CXX) $(CXXFLAGS) -o $(SERVER_TARGET) $(SERVER_SOURCES)

# Сборка SIEM-агента
$(SIEM_TARGET): $(SIEM_SOURCES)
	@echo "Building SIEM Agent..."
	$(CXX) $(CXXFLAGS) -I$(SIEMDIR)/include -o $(SIEM_TARGET) \
		$(SIEM_SOURCES) \
		$(SRCDIR)/utils.cpp \
		$(SRCDIR)/query_evaluator.cpp

# Очистка
clean:
	rm -f $(CLIENT_TARGET) $(SERVER_TARGET) $(SIEM_TARGET)
	rm -f databases/*.json 2>/dev/null || true

.PHONY: clean all
