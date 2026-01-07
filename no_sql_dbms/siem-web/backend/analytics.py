from typing import List, Dict, Any
from datetime import datetime, timedelta
from collections import Counter, defaultdict
from db_client import DBSocketClient

class AnalyticsService:

    def __init__(self, db_client: DBSocketClient):
        self.db = db_client

    def get_events_today(self, limit: int = 1000) -> List[Dict]:
        events = self.db.find_security_events({}, limit=limit)

        now = datetime.utcnow()
        yesterday = now - timedelta(days=1)

        recent_events = []
        for event in events:
            try:
                event_time = datetime.fromisoformat(event['timestamp'].replace('Z', '+00:00'))
                if event_time > yesterday:
                    recent_events.append(event)
            except:
                continue

        return recent_events

    def get_active_agents(self) -> List[Dict]:
        events = self.get_events_today()

        agents = {}
        for event in events:
            hostname = event.get('hostname', 'unknown')
            timestamp = event.get('timestamp', '')

            if hostname not in agents:
                agents[hostname] = {
                    'hostname': hostname,
                    'last_activity': timestamp,
                    'event_count': 0,
                    'last_event_type': event.get('event_type', ''),
                    'severity': event.get('severity', 'info')
                }

            if timestamp > agents[hostname]['last_activity']:
                agents[hostname]['last_activity'] = timestamp
                agents[hostname]['last_event_type'] = event.get('event_type', '')
                agents[hostname]['severity'] = event.get('severity', 'info')

            agents[hostname]['event_count'] += 1

        return list(agents.values())

    def get_auth_logs(self, limit: int = 10) -> List[Dict]:
        events = self.get_events_today()

        auth_events = []
        for event in events:
            event_type = event.get('event_type', '')
            if event_type in ['ssh_login', 'sudo_command', 'user_login', 'user_logout', 'auth_failure']:
                auth_events.append(event)

        return sorted(auth_events,
                     key=lambda x: x.get('timestamp', ''),
                     reverse=True)[:limit]

    def get_hosts_list(self) -> List[Dict]:
        events = self.get_events_today()

        hosts = defaultdict(int)
        for event in events:
            hostname = event.get('hostname', 'unknown')
            hosts[hostname] += 1

        return [{'hostname': host, 'event_count': count}
                for host, count in hosts.items()]

    def get_events_by_type(self) -> Dict[str, int]:
        events = self.get_events_today()

        type_counter = Counter()
        for event in events:
            event_type = event.get('event_type', 'unknown')
            type_counter[event_type] += 1

        return dict(type_counter)

    def get_events_by_severity(self) -> Dict[str, int]:
        events = self.get_events_today()

        severity_counter = Counter()
        for event in events:
            severity = event.get('severity', 'info')
            severity_counter[severity] += 1

        return dict(severity_counter)

    def get_top_users(self, limit: int = 10) -> List[Dict]:
        events = self.get_events_today()

        user_counter = Counter()
        for event in events:
            user = event.get('user', '')
            if user:
                user_counter[user] += 1

        return [{'user': user, 'event_count': count}
                for user, count in user_counter.most_common(limit)]

    def get_top_processes(self, limit: int = 10) -> List[Dict]:
        events = self.get_events_today()

        process_counter = Counter()
        for event in events:
            process = event.get('process', '')
            if process:
                process_counter[process] += 1

        return [{'process': process, 'event_count': count}
                for process, count in process_counter.most_common(limit)]

    def get_events_timeline(self) -> List[Dict]:
        events = self.get_events_today()

        timeline = defaultdict(int)
        for event in events:
            timestamp = event.get('timestamp', '')
            if timestamp:
                try:
                    hour = timestamp.split('T')[1].split(':')[0]
                    timeline[hour] += 1
                except:
                    continue

        sorted_timeline = sorted([{'hour': hour, 'count': count}
                                 for hour, count in timeline.items()])

        return sorted_timeline

    def get_events_today_count(self) -> int:
        return len(self.get_events_today())

    def get_critical_events_count(self) -> int:
        events = self.get_events_today()
        return sum(1 for e in events if e.get('severity') == 'high')

    def get_unique_hosts_count(self) -> int:
        events = self.get_events_today()
        hosts = set()
        for event in events:
            hosts.add(event.get('hostname', 'unknown'))
        return len(hosts)
