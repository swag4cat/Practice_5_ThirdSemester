class Dashboard {
    constructor() {
        this.charts = {};
        this.initCharts();
        this.loadDashboardData();
    }

    getAuthHeaders() {
        const token = sessionStorage.getItem('authToken');
        return {
            'Authorization': 'Basic ' + token,
            'Content-Type': 'application/json'
        };
    }

    async loadDashboardData() {
        try {
            const summaryResponse = await fetch('/api/dashboard/summary', {
                headers: this.getAuthHeaders()
            });

            if (!summaryResponse.ok) {
                throw new Error('Failed to fetch dashboard data');
            }

            const summary = await summaryResponse.json();

            const timelineResponse = await fetch('/api/events/stats/timeline', {
                headers: this.getAuthHeaders()
            });
            const timelineData = await timelineResponse.json();
            this.updateTimelineChart(timelineData);

            this.updateStats(summary);

            const typeResponse = await fetch('/api/events/stats/by-type', {
                headers: this.getAuthHeaders()
            });
            const typeData = await typeResponse.json();
            this.updateTypeChart(typeData);

            const severityResponse = await fetch('/api/events/stats/by-severity', {
                headers: this.getAuthHeaders()
            });
            const severityData = await severityResponse.json();
            this.updateSeverityChart(severityData);

            document.getElementById('lastUpdate').textContent =
                'Last updated: ' + new Date().toLocaleTimeString();

        } catch (error) {
            console.error('Error loading dashboard:', error);
            document.getElementById('apiStatus').className = 'badge bg-danger';
            document.getElementById('apiStatus').textContent = 'Disconnected';
        }
    }


    updateTimelineChart(data) {
        const hasData = data && Object.keys(data).length > 0;

        if (!hasData) {
            this.charts.timelineChart.data.datasets[0].data = Array(24).fill(0);
        } else {
            const labels = Object.keys(data);
            const values = Object.values(data);

            this.charts.timelineChart.data.labels = labels;
            this.charts.timelineChart.data.datasets[0].data = values;
        }

        this.charts.timelineChart.update();
    }

    updateStats(summary) {
        document.getElementById('activeAgentsCount').textContent =
            summary.active_agents?.length || 0;
        document.getElementById('eventsTodayCount').textContent =
            summary.events_today || 0;
        document.getElementById('criticalEventsCount').textContent =
            summary.critical_events || 0;
        document.getElementById('uniqueHostsCount').textContent =
            summary.unique_hosts || 0;

        this.updateAgentsTable(summary.active_agents || []);

        this.updateAuthTable(summary.auth_logs || []);

        this.updateTopUsersTable(summary.top_users || []);

        this.updateTopProcessesTable(summary.top_processes || []);

        document.getElementById('agentsCount').textContent =
            summary.active_agents?.length || 0;
        document.getElementById('eventsCount').textContent =
            summary.events_today || 0;
    }

    updateAgentsTable(agents) {
        const tbody = document.getElementById('agentsTable');
        tbody.innerHTML = '';

        if (!agents || agents.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="4" class="text-center py-4">
                        <i class="fas fa-server fa-2x mb-3 text-muted"></i>
                        <p class="text-muted mb-0">No active agents detected</p>
                        <small class="text-muted">Waiting for agent connections...</small>
                    </td>
                </tr>
            `;
            return;
        }

        agents.forEach(agent => {
            const timeAgo = this.getTimeAgo(agent.last_activity);
            const row = `
                <tr>
                    <td><i class="fas fa-server me-2"></i>${agent.hostname}</td>
                    <td>${timeAgo}</td>
                    <td><span class="badge bg-primary">${agent.event_count}</span></td>
                    <td><span class="badge bg-success">Active</span></td>
                </tr>
            `;
            tbody.innerHTML += row;
        });
    }

    updateTopUsersTable(users) {
        const tbody = document.getElementById('topUsersTable');
        tbody.innerHTML = '';

        if (!users || users.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="3" class="text-center py-4">
                        <i class="fas fa-user-friends fa-2x mb-3 text-muted"></i>
                        <p class="text-muted mb-0">No user activity data</p>
                        <small class="text-muted">Waiting for user events...</small>
                    </td>
                </tr>
            `;
            return;
        }

        users.forEach(user => {
            const lastActivity = user.last_activity ?
                this.getTimeAgo(user.last_activity) : 'N/A';

            const row = `
                <tr>
                    <td><i class="fas fa-user me-2"></i>${user.user || 'Unknown'}</td>
                    <td><span class="badge bg-primary">${user.count}</span></td>
                    <td><small class="text-muted">${lastActivity}</small></td>
                </tr>
            `;
            tbody.innerHTML += row;
        });
    }

    updateTopProcessesTable(processes) {
        const tbody = document.getElementById('topProcessesTable');
        tbody.innerHTML = '';

        if (!processes || processes.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="3" class="text-center py-4">
                        <i class="fas fa-microchip fa-2x mb-3 text-muted"></i>
                        <p class="text-muted mb-0">No process data</p>
                        <small class="text-muted">Waiting for process events...</small>
                    </td>
                </tr>
            `;
            return;
        }

        processes.forEach(proc => {
            const row = `
                <tr>
                    <td><i class="fas fa-cogs me-2"></i>${proc.process || 'Unknown'}</td>
                    <td><span class="badge bg-info">${proc.count}</span></td>
                    <td><small><i class="fas fa-server me-1"></i> ${proc.most_active_host || 'N/A'}</small></td>
                </tr>
            `;
            tbody.innerHTML += row;
        });
    }

    updateAuthTable(events) {
        const tbody = document.getElementById('authEventsTable');
        tbody.innerHTML = '';

        if (!events || events.length === 0) {
            tbody.innerHTML = `
                <tr>
                    <td colspan="4" class="text-center py-4">
                        <i class="fas fa-user-lock fa-2x mb-3 text-muted"></i>
                        <p class="text-muted mb-0">No authentication events</p>
                        <small class="text-muted">Waiting for login activity...</small>
                    </td>
                </tr>
            `;
            return;
        }

        events.forEach(event => {
            const time = event.timestamp ?
                new Date(event.timestamp).toLocaleTimeString() : 'N/A';

            const severityClass = this.getSeverityClass(event.severity);

            const row = `
                <tr>
                    <td><small>${time}</small></td>
                    <td><strong>${event.user || 'N/A'}</strong></td>
                    <td>${event.event_type || 'Unknown'}</td>
                    <td><span class="badge ${severityClass}">${event.severity || 'info'}</span></td>
                </tr>
            `;
            tbody.innerHTML += row;
        });
    }

    initCharts() {
        const typeCtx = document.getElementById('eventsByTypeChart').getContext('2d');
        this.charts.typeChart = new Chart(typeCtx, {
            type: 'doughnut',
            data: {
                labels: ['No data yet'],
                datasets: [{
                    data: [1],
                    backgroundColor: ['#f0f0f0'],
                    borderColor: ['#e0e0e0'],
                    borderWidth: 1
                }]
            },
            options: {
                responsive: true,
                plugins: {
                    legend: {
                        position: 'bottom',
                        labels: {
                            color: '#666'
                        }
                    },
                    tooltip: {
                        enabled: false
                    }
                },
                cutout: '60%'
            },
            plugins: [{
                id: 'no-data',
                beforeDraw: function(chart) {
                    if (chart.data.datasets[0].data[0] === 1 && chart.data.labels[0] === 'No data yet') {
                        const ctx = chart.ctx;
                        const width = chart.width;
                        const height = chart.height;

                        ctx.save();
                        ctx.textAlign = 'center';
                        ctx.textBaseline = 'middle';
                        ctx.font = '14px "Segoe UI", sans-serif';
                        ctx.fillStyle = '#999';
                        ctx.fillText('Waiting for events...', width / 2, height / 2);
                        ctx.restore();
                    }
                }
            }]
        });

        const severityCtx = document.getElementById('eventsBySeverityChart').getContext('2d');
        this.charts.severityChart = new Chart(severityCtx, {
            type: 'bar',
            data: {
                labels: ['No data'],
                datasets: [{
                    label: 'Events',
                    data: [0],
                    backgroundColor: '#f0f0f0',
                    borderColor: '#e0e0e0',
                    borderWidth: 1
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: true,
                        ticks: {
                            color: '#666'
                        },
                        grid: {
                            color: '#f5f5f5'
                        }
                    },
                    x: {
                        ticks: {
                            color: '#666'
                        },
                        grid: {
                            display: false
                        }
                    }
                },
                plugins: {
                    legend: {
                        display: false
                    },
                    tooltip: {
                        enabled: false
                    }
                }
            },
            plugins: [{
                id: 'no-data',
                beforeDraw: function(chart) {
                    if (chart.data.datasets[0].data[0] === 0 && chart.data.labels[0] === 'No data') {
                        const ctx = chart.ctx;
                        const width = chart.width;
                        const height = chart.height;

                        ctx.save();
                        ctx.textAlign = 'center';
                        ctx.textBaseline = 'middle';
                        ctx.font = '14px "Segoe UI", sans-serif';
                        ctx.fillStyle = '#999';
                        ctx.fillText('Waiting for events...', width / 2, height / 2);
                        ctx.restore();
                    }
                }
            }]
        });

        const timelineCtx = document.getElementById('eventsTimelineChart').getContext('2d');
        this.charts.timelineChart = new Chart(timelineCtx, {
            type: 'line',
            data: {
                labels: Array.from({length: 24}, (_, i) => `${i.toString().padStart(2, '0')}:00`),
                datasets: [{
                    label: 'Events per hour',
                    data: Array(24).fill(0),
                    borderColor: '#3498db',
                    backgroundColor: 'rgba(52, 152, 219, 0.1)',
                    borderWidth: 2,
                    fill: true,
                    tension: 0.4,
                    pointBackgroundColor: '#3498db',
                    pointBorderColor: '#fff',
                    pointBorderWidth: 2,
                    pointRadius: 4
                }]
            },
            options: {
                responsive: true,
                scales: {
                    y: {
                        beginAtZero: true,
                        ticks: {
                            color: '#666'
                        },
                        grid: {
                            color: '#f5f5f5'
                        },
                        title: {
                            display: true,
                            text: 'Number of events',
                            color: '#666'
                        }
                    },
                    x: {
                        ticks: {
                            color: '#666',
                            maxRotation: 45
                        },
                        grid: {
                            color: '#f5f5f5'
                        },
                        title: {
                            display: true,
                            text: 'Time (hour)',
                            color: '#666'
                        }
                    }
                },
                plugins: {
                    legend: {
                        display: true,
                        position: 'top',
                        labels: {
                            color: '#666'
                        }
                    },
                    tooltip: {
                        mode: 'index',
                        intersect: false,
                        backgroundColor: 'rgba(0, 0, 0, 0.7)',
                        titleColor: '#fff',
                        bodyColor: '#fff'
                    }
                }
            }
        });
    }

    updateTypeChart(data) {
        const hasData = data && Object.keys(data).length > 0 &&
        !data.message;

        if (!hasData) {
            this.charts.typeChart.data.labels = ['No data yet'];
            this.charts.typeChart.data.datasets[0].data = [1];
            this.charts.typeChart.data.datasets[0].backgroundColor = ['#f0f0f0'];
        } else {
            const labels = Object.keys(data);
            const values = Object.values(data);

            const colors = [
                '#3498db', '#2ecc71', '#e74c3c', '#f39c12',
                '#9b59b6', '#1abc9c', '#d35400', '#34495e'
            ];

            this.charts.typeChart.data.labels = labels;
            this.charts.typeChart.data.datasets[0].data = values;
            this.charts.typeChart.data.datasets[0].backgroundColor = colors.slice(0, labels.length);
        }

        this.charts.typeChart.options.plugins.tooltip.enabled = hasData;
        this.charts.typeChart.update();
    }

    updateSeverityChart(data) {
        const hasData = data && Object.keys(data).length > 0 &&
        !data.message;

        if (!hasData) {
            this.charts.severityChart.data.labels = ['No data'];
            this.charts.severityChart.data.datasets[0].data = [0];
            this.charts.severityChart.data.datasets[0].backgroundColor = '#f0f0f0';
        } else {
            const labels = Object.keys(data).map(key =>
            key.charAt(0).toUpperCase() + key.slice(1)
            );
            const values = Object.values(data);

            const colors = {
                'Critical': '#e74c3c',
                'High': '#e67e22',
                'Medium': '#f39c12',
                'Low': '#27ae60',
                'Info': '#3498db'
            };

            const backgroundColors = labels.map(label =>
            colors[label] || '#95a5a6'
            );

            this.charts.severityChart.data.labels = labels;
            this.charts.severityChart.data.datasets[0].data = values;
            this.charts.severityChart.data.datasets[0].backgroundColor = backgroundColors;
        }

        this.charts.severityChart.options.plugins.tooltip.enabled = hasData;
        this.charts.severityChart.update();
    }

    getTimeAgo(timestamp) {
        if (!timestamp) return 'Unknown';

        const now = new Date();
        const past = new Date(timestamp);
        const diff = now - past;

        const minutes = Math.floor(diff / 60000);
        const hours = Math.floor(minutes / 60);

        if (minutes < 1) return 'Just now';
        if (minutes < 60) return `${minutes}m ago`;
        if (hours < 24) return `${hours}h ago`;

        return past.toLocaleDateString();
    }

    getSeverityClass(severity) {
        switch(severity?.toLowerCase()) {
            case 'critical': return 'badge-critical';
            case 'high': return 'badge-high';
            case 'medium': return 'badge-medium';
            case 'low': return 'badge-low';
            default: return 'badge-info';
        }
    }
}

document.addEventListener('DOMContentLoaded', () => {
    window.dashboard = new Dashboard();
});

function updateDashboard() {
    if (window.dashboard) {
        window.dashboard.loadDashboardData();
    }
}
