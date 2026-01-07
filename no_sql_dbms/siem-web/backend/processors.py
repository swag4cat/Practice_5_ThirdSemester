from typing import List, Dict, Any
from collections import Counter, defaultdict
from datetime import datetime, timedelta

def process_real_events(events: List[Dict]) -> Dict[str, Any]:

    if not events:
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
            "db_connected": True
        }

    recent_events = events

    print(f"[Processor] Processing ALL {len(recent_events)} events")

    agents = {}
    auth_events = []
    host_counter = Counter()
    type_counter = Counter()
    severity_counter = Counter()
    user_counter = Counter()
    process_counter = Counter()

    process_hosts = defaultdict(lambda: defaultdict(int))
    user_last_activity = {}

    for event in recent_events:
        hostname = event.get('hostname', 'unknown')
        event_type = event.get('event_type', 'unknown')
        severity = event.get('severity', 'info')
        user = event.get('user', '')
        process_name = event.get('process', '')
        timestamp = event.get('timestamp', '')

        if len(agents) == 0:
            print(f"[Processor] First event: {event}")

        if hostname not in agents:
            agents[hostname] = {
                'hostname': hostname,
                'last_activity': timestamp,
                'event_count': 0
            }

        if timestamp:
            if not agents[hostname]['last_activity'] or timestamp > agents[hostname]['last_activity']:
                agents[hostname]['last_activity'] = timestamp

        agents[hostname]['event_count'] += 1

        if event_type in ['ssh_login', 'sudo_command', 'user_login', 'user_logout', 'auth_failure',
                         'ssh_failed_login', 'ssh_disconnect']:
            auth_events.append(event)

        host_counter[hostname] += 1
        type_counter[event_type] += 1
        severity_counter[severity] += 1

        if user:
            user_counter[user] += 1
            if user not in user_last_activity or timestamp > user_last_activity.get(user, ''):
                user_last_activity[user] = timestamp

        if process_name:
            process_counter[process_name] += 1
            if hostname:
                process_hosts[process_name][hostname] += 1

    sorted_agents = sorted(
        agents.values(),
        key=lambda x: x.get('last_activity', ''),
        reverse=True
    )

    sorted_auth_events = sorted(
        auth_events,
        key=lambda x: x.get('timestamp', ''),
        reverse=True
    )[:10]

    top_users_with_activity = []
    for user, count in user_counter.most_common(10):
        last_activity = user_last_activity.get(user, "")
        top_users_with_activity.append({
            "user": user,
            "count": count,
            "last_activity": last_activity
        })

    top_processes_with_host = []
    for process, count in process_counter.most_common(10):
        hosts_for_process = process_hosts.get(process, {})
        if hosts_for_process:
            most_active_host = max(hosts_for_process.items(), key=lambda x: x[1])[0]
        else:
            most_active_host = "N/A"

        top_processes_with_host.append({
            "process": process,
            "count": count,
            "most_active_host": most_active_host
        })

    print(f"[Processor] Found {len(sorted_agents)} agents: {[a['hostname'] for a in sorted_agents]}")
    print(f"[Processor] Found {len(sorted_auth_events)} auth events")
    print(f"[Processor] Top users: {top_users_with_activity}")
    print(f"[Processor] Top processes: {top_processes_with_host}")

    return {
        "active_agents": sorted_agents,
        "auth_logs": sorted_auth_events,
        "events_today": len(recent_events),
        "critical_events": severity_counter.get('high', 0) + severity_counter.get('critical', 0),
        "unique_hosts": len(host_counter),
        "events_by_type": dict(type_counter),
        "events_by_severity": dict(severity_counter),
        "top_users": top_users_with_activity,
        "top_processes": top_processes_with_host,
        "db_connected": True,
        "total_events_analyzed": len(events)
    }
