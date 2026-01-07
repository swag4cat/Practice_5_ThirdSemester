#!/bin/bash

# install.sh - Установка SIEM Agent как systemd сервиса

set -e

echo "=== Установка SIEM Agent ==="

# 1. Создаем директории
echo "Создаем директории..."
sudo mkdir -p /opt/siem-agent/{bin,configs,logs}
sudo mkdir -p /var/lib/siem-agent
sudo mkdir -p /etc/siem-agent

# 2. Копируем бинарник
echo "Копируем бинарник..."
sudo cp siem_agent /opt/siem-agent/bin/
sudo chmod +x /opt/siem-agent/bin/siem_agent

# 3. Копируем конфиг
echo "Копируем конфигурацию..."
sudo cp configs/agent_config.json /etc/siem-agent/config.json

# 4. Копируем systemd unit
echo "Настраиваем systemd..."
sudo cp systemd/siem-agent.service /etc/systemd/system/

# 5. Настраиваем права
echo "Настраиваем права..."
sudo chown -R root:root /opt/siem-agent
sudo chown -R root:root /var/lib/siem-agent
sudo chown root:root /etc/siem-agent/config.json

# 6. Включаем автозапуск
echo "Включаем автозапуск..."
sudo systemctl daemon-reload
sudo systemctl enable siem-agent.service

echo ""
echo "=== Установка завершена ==="
echo ""
echo "Команды управления:"
echo "  sudo systemctl start siem-agent     # Запустить"
echo "  sudo systemctl stop siem-agent      # Остановить"
echo "  sudo systemctl status siem-agent    # Статус"
echo "  sudo journalctl -u siem-agent -f    # Логи"
echo ""
echo "Агент автоматически запустится при загрузке системы."
