document.addEventListener('DOMContentLoaded', function() {
    const loginForm = document.getElementById('loginForm');
    const errorAlert = document.getElementById('errorAlert');

    if (sessionStorage.getItem('authToken')) {
        window.location.href = '/dashboard';
    }

    loginForm.addEventListener('submit', async function(e) {
        e.preventDefault();

        const username = document.getElementById('username').value.trim();
        const password = document.getElementById('password').value;

        const submitBtn = loginForm.querySelector('button[type="submit"]');
        const originalText = submitBtn.innerHTML;
        submitBtn.innerHTML = '<i class="fas fa-spinner fa-spin"></i> Authenticating...';
        submitBtn.disabled = true;

        try {
            const response = await fetch('/api/dashboard/summary', {
                method: 'GET',
                headers: {
                    'Authorization': 'Basic ' + btoa(username + ':' + password)
                }
            });

            if (response.ok) {

                sessionStorage.setItem('authToken', btoa(username + ':' + password));
                sessionStorage.setItem('username', username);

                errorAlert.classList.remove('alert-danger');
                errorAlert.classList.add('alert-success');
                errorAlert.textContent = '✓ Authentication successful! Redirecting...';
                errorAlert.classList.remove('d-none');

                setTimeout(() => {
                    window.location.href = '/dashboard';
                }, 1000);

            } else {
                throw new Error('Invalid credentials');
            }

        } catch (error) {
            errorAlert.classList.remove('alert-success');
            errorAlert.classList.add('alert-danger');
            errorAlert.textContent = '✗ Authentication failed. Please check your credentials.';
            errorAlert.classList.remove('d-none');

            errorAlert.style.animation = 'none';
            setTimeout(() => {
                errorAlert.style.animation = 'shake 0.5s';
            }, 10);

            document.getElementById('username').focus();

        } finally {
            submitBtn.innerHTML = originalText;
            submitBtn.disabled = false;
        }
    });

    if (window.location.hostname === 'localhost' || window.location.hostname === '127.0.0.1') {
        document.getElementById('username').value = 'admin';
        document.getElementById('password').value = 'admin123';
    }
});

const style = document.createElement('style');
style.textContent = `
    @keyframes shake {
        0%, 100% { transform: translateX(0); }
        10%, 30%, 50%, 70%, 90% { transform: translateX(-5px); }
        20%, 40%, 60%, 80% { transform: translateX(5px); }
    }
`;
document.head.appendChild(style);
