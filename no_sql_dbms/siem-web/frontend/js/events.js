class EventsRegistry {
    constructor() {
        this.currentPage = 1;
        this.pageSize = 20;
        this.totalEvents = 0;
        this.filters = {};
        this.allEvents = [];
        this.isLoading = false;
        this.init();
    }

    getAuthHeaders() {
        const token = sessionStorage.getItem('authToken');
        return {
            'Authorization': 'Basic ' + token,
            'Content-Type': 'application/json'
        };
    }

    async init() {
        if (!sessionStorage.getItem('authToken')) {
            window.location.href = '/';
            return;
        }

        document.getElementById('filterType').addEventListener('change', () => this.searchEvents());
        document.getElementById('filterSeverity').addEventListener('change', () => this.searchEvents());
        document.getElementById('filterHost').addEventListener('input', () => this.searchEvents());

        document.querySelector('button[onclick="loadEvents()"]').addEventListener('click', (e) => {
            e.preventDefault();
            this.searchEvents();
        });

        document.getElementById('filterHost').addEventListener('keypress', (e) => {
            if (e.key === 'Enter') {
                this.searchEvents();
            }
        });

        await this.loadEvents();
    }

    async loadEvents(page = 1) {
        if (this.isLoading) return;

        this.isLoading = true;
        this.currentPage = page;

        document.getElementById('eventsList').innerHTML = `
            <div class="text-center py-5">
                <div class="spinner-border text-primary" role="status">
                    <span class="visually-hidden">Loading...</span>
                </div>
                <p class="mt-3 text-muted">Loading security events...</p>
            </div>
        `;

        try {
            const response = await fetch(`/api/events?skip=0&limit=1000`, {
                headers: this.getAuthHeaders()
            });

            if (!response.ok) {
                if (response.status === 401) {
                    window.location.href = '/';
                    return;
                }
                throw new Error(`Server error: ${response.status}`);
            }

            const data = await response.json();
            this.allEvents = data.events || [];
            this.applyFilters();

        } catch (error) {
            console.error('Error loading events:', error);
            this.showError(`Failed to load events: ${error.message}`);
        } finally {
            this.isLoading = false;
        }
    }

    applyFilters() {
        const typeFilter = document.getElementById('filterType').value.toLowerCase();
        const severityFilter = document.getElementById('filterSeverity').value.toLowerCase();
        const hostFilter = document.getElementById('filterHost').value.toLowerCase().trim();

        let filteredEvents = this.allEvents;

        if (typeFilter) {
            filteredEvents = filteredEvents.filter(event =>
                event.event_type?.toLowerCase().includes(typeFilter)
            );
        }

        if (severityFilter) {
            filteredEvents = filteredEvents.filter(event =>
                event.severity?.toLowerCase() === severityFilter
            );
        }

        if (hostFilter) {
            filteredEvents = filteredEvents.filter(event =>
                event.hostname?.toLowerCase().includes(hostFilter) ||
                event.source?.toLowerCase().includes(hostFilter)
            );
        }

        this.totalEvents = filteredEvents.length;
        const start = (this.currentPage - 1) * this.pageSize;
        const end = start + this.pageSize;
        const pageEvents = filteredEvents.slice(start, end);

        this.renderEvents(pageEvents);
        this.renderPagination();
    }

    searchEvents() {
        this.currentPage = 1;
        this.applyFilters();
    }

    renderEvents(events) {
        const container = document.getElementById('eventsList');

        if (!events || events.length === 0) {
            container.innerHTML = `
                <div class="alert alert-info fade-in">
                    <div class="d-flex align-items-center">
                        <i class="fas fa-database fa-3x me-4 text-muted"></i>
                        <div>
                            <h6 class="mb-1">No security events found</h6>
                            <p class="mb-0">
                                <i class="fas fa-info-circle me-1"></i>
                                The database is connected but no security events have been collected yet.
                            </p>
                            <div class="mt-3">
                                <div class="d-flex gap-2">
                                    <button class="btn btn-sm btn-outline-primary" onclick="registry.loadEvents()">
                                        <i class="fas fa-redo me-1"></i> Refresh
                                    </button>
                                    <button class="btn btn-sm btn-outline-secondary" onclick="registry.simulateTestEvents()">
                                        <i class="fas fa-bolt me-1"></i> Add Test Events
                                    </button>
                                </div>
                                <small class="text-muted d-block mt-2">
                                    <i class="fas fa-lightbulb me-1"></i>
                                    Start your SIEM agent or add test logs to see events here.
                                </small>
                            </div>
                        </div>
                    </div>
                </div>
            `;
            return;
        }


        let html = '';
        events.forEach((event, index) => {
            const time = event.timestamp ?
                new Date(event.timestamp).toLocaleString() : 'Unknown';

            const severityClass = this.getSeverityClass(event.severity);
            const severityText = event.severity ?
                event.severity.charAt(0).toUpperCase() + event.severity.slice(1) : 'Info';

            html += `
                <div class="event-row cursor-pointer" onclick="registry.toggleDetails(${index})">
                    <div class="row align-items-center">
                        <div class="col-md-2">
                            <small class="text-muted">${time}</small>
                        </div>
                        <div class="col-md-2">
                            <span class="badge ${severityClass}">
                                ${severityText}
                            </span>
                        </div>
                        <div class="col-md-2">
                            <strong>${event.event_type || 'Unknown'}</strong>
                        </div>
                        <div class="col-md-3">
                            <i class="fas fa-server me-1"></i> ${event.hostname || 'Unknown'}
                            ${event.source ? `<br><small class="text-muted">Source: ${event.source}</small>` : ''}
                        </div>
                        <div class="col-md-2">
                            <i class="fas fa-user me-1"></i> ${event.user || 'N/A'}
                        </div>
                        <div class="col-md-1 text-end">
                            <i class="fas fa-chevron-down" id="chevron${index}"></i>
                        </div>
                    </div>

                    <div class="event-details mt-3" id="eventDetails${index}" style="display: none;">
                        <div class="card border-0 bg-light">
                            <div class="card-body">
                                <h6><i class="fas fa-info-circle me-2"></i> Event Details</h6>
                                <div class="row">
                                    <div class="col-md-6">
                                        <table class="table table-sm table-borderless">
                                            <tr>
                                                <th width="120">Event ID:</th>
                                                <td><code>${event._id || 'N/A'}</code></td>
                                            </tr>
                                            <tr>
                                                <th>Source:</th>
                                                <td>${event.source || 'N/A'}</td>
                                            </tr>
                                            <tr>
                                                <th>Process:</th>
                                                <td>${event.process || 'N/A'}</td>
                                            </tr>
                                            <tr>
                                                <th>Command:</th>
                                                <td><code class="bg-white p-1 rounded">${event.command || 'N/A'}</code></td>
                                            </tr>
                                            <tr>
                                                <th>User:</th>
                                                <td>${event.user || 'N/A'}</td>
                                            </tr>
                                            <tr>
                                                <th>Hostname:</th>
                                                <td>${event.hostname || 'N/A'}</td>
                                            </tr>
                                        </table>
                                    </div>
                                    <div class="col-md-6">
                                        <strong><i class="fas fa-file-alt me-1"></i> Raw Log:</strong>
                                        <pre class="mt-2 p-3 bg-white rounded border" style="font-size: 12px; max-height: 150px; overflow-y: auto;">${event.raw_log || 'No raw log available'}</pre>
                                    </div>
                                </div>
                                <div class="d-flex gap-2 mt-3">
                                    <button class="btn btn-sm btn-outline-primary" onclick="event.stopPropagation(); registry.exportEvent(${index})">
                                        <i class="fas fa-download me-1"></i> Export as JSON
                                    </button>
                                    <button class="btn btn-sm btn-outline-secondary" onclick="event.stopPropagation(); registry.copyEventId(${index})">
                                        <i class="fas fa-copy me-1"></i> Copy Event ID
                                    </button>
                                </div>
                            </div>
                        </div>
                    </div>
                </div>
            `;
        });

        container.innerHTML = html;
    }

    renderPagination() {
        const totalPages = Math.ceil(this.totalEvents / this.pageSize);
        const pagination = document.getElementById('pagination');

        if (totalPages <= 1) {
            pagination.innerHTML = '';
            return;
        }

        let html = '';

        if (this.currentPage > 1) {
            html += `
                <li class="page-item">
                    <a class="page-link" href="#" onclick="registry.loadEvents(${this.currentPage - 1}); return false;">
                        <i class="fas fa-chevron-left"></i>
                    </a>
                </li>
            `;
        } else {
            html += `
                <li class="page-item disabled">
                    <span class="page-link">
                        <i class="fas fa-chevron-left"></i>
                    </span>
                </li>
            `;
        }

        const maxPagesToShow = 5;
        let startPage = Math.max(1, this.currentPage - Math.floor(maxPagesToShow / 2));
        let endPage = Math.min(totalPages, startPage + maxPagesToShow - 1);

        if (endPage - startPage + 1 < maxPagesToShow) {
            startPage = Math.max(1, endPage - maxPagesToShow + 1);
        }

        if (startPage > 1) {
            html += `
                <li class="page-item">
                    <a class="page-link" href="#" onclick="registry.loadEvents(1); return false;">1</a>
                </li>
            `;
            if (startPage > 2) {
                html += `<li class="page-item disabled"><span class="page-link">...</span></li>`;
            }
        }

        for (let i = startPage; i <= endPage; i++) {
            if (i === this.currentPage) {
                html += `<li class="page-item active"><span class="page-link">${i}</span></li>`;
            } else {
                html += `
                    <li class="page-item">
                        <a class="page-link" href="#" onclick="registry.loadEvents(${i}); return false;">${i}</a>
                    </li>
                `;
            }
        }

        if (endPage < totalPages) {
            if (endPage < totalPages - 1) {
                html += `<li class="page-item disabled"><span class="page-link">...</span></li>`;
            }
            html += `
                <li class="page-item">
                    <a class="page-link" href="#" onclick="registry.loadEvents(${totalPages}); return false;">${totalPages}</a>
                </li>
            `;
        }

        if (this.currentPage < totalPages) {
            html += `
                <li class="page-item">
                    <a class="page-link" href="#" onclick="registry.loadEvents(${this.currentPage + 1}); return false;">
                        <i class="fas fa-chevron-right"></i>
                    </a>
                </li>
            `;
        } else {
            html += `
                <li class="page-item disabled">
                    <span class="page-link">
                        <i class="fas fa-chevron-right"></i>
                    </span>
                </li>
            `;
        }

        html += `
            <li class="page-item disabled ms-3">
                <span class="page-link border-0 bg-transparent">
                    <small>${this.totalEvents} events • Page ${this.currentPage} of ${totalPages}</small>
                </span>
            </li>
        `;

        pagination.innerHTML = html;
    }

    toggleDetails(index) {
        const details = document.getElementById(`eventDetails${index}`);
        const chevron = document.getElementById(`chevron${index}`);

        if (details.style.display === 'block') {
            details.style.display = 'none';
            chevron.className = 'fas fa-chevron-down';
        } else {
            details.style.display = 'block';
            chevron.className = 'fas fa-chevron-up';

            document.querySelectorAll('.event-details').forEach((el, i) => {
                if (i !== index && el.style.display === 'block') {
                    el.style.display = 'none';
                    const otherChevron = document.getElementById(`chevron${i}`);
                    if (otherChevron) otherChevron.className = 'fas fa-chevron-down';
                }
            });
        }
    }

    exportEvent(index) {
        const events = this.allEvents;
        if (index >= events.length) return;

        const eventData = events[index];
        const dataStr = JSON.stringify(eventData, null, 2);
        const dataUri = 'data:application/json;charset=utf-8,'+ encodeURIComponent(dataStr);

        const exportFileDefaultName = `siem-event-${eventData._id || Date.now()}.json`;

        const linkElement = document.createElement('a');
        linkElement.setAttribute('href', dataUri);
        linkElement.setAttribute('download', exportFileDefaultName);
        linkElement.click();

        this.showToast('Event exported successfully!', 'success');
    }

    copyEventId(index) {
        const events = this.allEvents;
        if (index >= events.length || !events[index]._id) return;

        navigator.clipboard.writeText(events[index]._id)
            .then(() => this.showToast('Event ID copied to clipboard!', 'success'))
            .catch(() => this.showToast('Failed to copy Event ID', 'error'));
    }

    clearFilters() {
        document.getElementById('filterType').value = '';
        document.getElementById('filterSeverity').value = '';
        document.getElementById('filterHost').value = '';
        this.searchEvents();
    }

    showError(message) {
        document.getElementById('eventsList').innerHTML = `
            <div class="alert alert-danger fade-in shake">
                <div class="d-flex align-items-center">
                    <i class="fas fa-exclamation-triangle me-3 fa-lg"></i>
                    <div>
                        <h6 class="mb-1">Error loading events</h6>
                        <p class="mb-0">${message}</p>
                        <button class="btn btn-sm btn-outline-danger mt-2" onclick="registry.loadEvents()">
                            <i class="fas fa-redo me-1"></i> Try Again
                        </button>
                    </div>
                </div>
            </div>
        `;
    }

    showToast(message, type = 'info') {
        const toast = document.createElement('div');
        toast.className = `toast align-items-center text-white bg-${type === 'success' ? 'success' : 'danger'} border-0`;
        toast.setAttribute('role', 'alert');
        toast.setAttribute('aria-live', 'assertive');
        toast.setAttribute('aria-atomic', 'true');

        toast.innerHTML = `
            <div class="d-flex">
                <div class="toast-body">
                    <i class="fas fa-${type === 'success' ? 'check-circle' : 'exclamation-circle'} me-2"></i>
                    ${message}
                </div>
                <button type="button" class="btn-close btn-close-white me-2 m-auto" data-bs-dismiss="toast"></button>
            </div>
        `;

        const container = document.getElementById('toastContainer') || (() => {
            const div = document.createElement('div');
            div.id = 'toastContainer';
            div.className = 'toast-container position-fixed bottom-0 end-0 p-3';
            div.style.zIndex = '1050';
            document.body.appendChild(div);
            return div;
        })();

        container.appendChild(toast);

        const bsToast = new bootstrap.Toast(toast);
        bsToast.show();

        toast.addEventListener('hidden.bs.toast', () => {
            toast.remove();
        });
    }

    getSeverityClass(severity) {
        if (!severity) return 'badge-info';

        switch(severity.toLowerCase()) {
            case 'critical': return 'badge-critical';
            case 'high': return 'badge-high';
            case 'medium': return 'badge-medium';
            case 'low': return 'badge-low';
            default: return 'badge-info';
        }
    }
}

document.addEventListener('DOMContentLoaded', () => {
    window.registry = new EventsRegistry();

    if (typeof bootstrap === 'undefined') {
        const script = document.createElement('script');
        script.src = 'https://cdn.jsdelivr.net/npm/bootstrap@5.3.0/dist/js/bootstrap.bundle.min.js';
        document.head.appendChild(script);
    }
});
