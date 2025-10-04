document.getElementById('loginForm').onsubmit = async (e) => {
    e.preventDefault();

    const res = await fetch('/auth/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: new URLSearchParams({
            password: document.getElementById('password').value,
            totp: document.getElementById('totp').value || ''
        })
    });

    if (res.ok) {
        const data = await res.json();
        sessionStorage.setItem('token', data.token);
        window.location.href = '/';
    } else {
        alert('Invalid credentials');
    }
};