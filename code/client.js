let ws = null;
let isLoggedIn = false;
let username = null;
let isConnecting = false;
let reconnectAttempts = 0;
const maxReconnectAttempts = 5;

function connect() {
    if (isConnecting || reconnectAttempts >= maxReconnectAttempts) {
        console.log(`Connect skipped: isConnecting=${isConnecting}, attempts=${reconnectAttempts}`);
        if (reconnectAttempts >= maxReconnectAttempts) {
            addMessage('System: Reconnect failed after maximum attempts');
        }
        return;
    }
    isConnecting = true;
    reconnectAttempts++;
    console.log(`Attempting to connect, attempt ${reconnectAttempts} at ${new Date().toLocaleTimeString()}`);
    addMessage('System: Connecting...');
    // Отключаем кнопки во время подключения
    document.getElementById('loginButton').disabled = true;
    document.getElementById('registerButton').disabled = true;
    document.getElementById('sendButton').disabled = true;
    ws = new WebSocket('ws://127.0.0.1:8080');
    ws.onopen = () => {
        isConnecting = false;
        reconnectAttempts = 0; // Сбрасываем попытки после успеха
        console.log(`Connected to server at ${new Date().toLocaleTimeString()}!`);
        addMessage('System: Connected to server');
        // Включаем кнопки после успешного подключения
        document.getElementById('loginButton').disabled = false;
        document.getElementById('registerButton').disabled = false;
        document.getElementById('sendButton').disabled = isLoggedIn ? false : true;
    };
    ws.onmessage = (event) => {
        console.log('Received message:', event.data);
        if (event.data.startsWith('System: Login successful')) {
            isLoggedIn = true;
            console.log('Login successful, setting isLoggedIn to true');
            document.getElementById('logoutButton').style.display = 'inline';
            document.getElementById('sendButton').disabled = false;
        } else if (event.data.startsWith('System: Logout successful')) {
            isLoggedIn = false;
            username = null;
            console.log('Logout successful, resetting state');
            document.getElementById('logoutButton').style.display = 'none';
            document.getElementById('login').value = '';
            document.getElementById('password').value = '';
            document.getElementById('sendButton').disabled = true;
            isConnecting = false; // Сбрасываем перед переподключением
            setTimeout(connect, 1000); // Уменьшена задержка до 1 секунды
        }
        addMessage(event.data);
    };
    ws.onclose = () => {
        console.log(`Disconnected from server at ${new Date().toLocaleTimeString()}`);
        addMessage('System: Disconnected from server');
        isLoggedIn = false;
        username = null;
        document.getElementById('logoutButton').style.display = 'none';
        document.getElementById('sendButton').disabled = true;
        // Отключаем кнопки во время переподключения
        document.getElementById('loginButton').disabled = true;
        document.getElementById('registerButton').disabled = true;
        isConnecting = false; // Сбрасываем перед переподключением
        setTimeout(connect, 1000); // Уменьшена задержка до 1 секунды
    };
    ws.onerror = (error) => {
        console.error('WebSocket error:', error);
        addMessage(`System: WebSocket error occurred: ${error.message || 'Connection failed'}`);
        // Отключаем кнопки при ошибке
        document.getElementById('loginButton').disabled = true;
        document.getElementById('registerButton').disabled = true;
        document.getElementById('sendButton').disabled = true;
        isConnecting = false; // Сбрасываем перед переподключением
        setTimeout(connect, 1000); // Уменьшена задержка до 1 секунды
    };
}

function register() {
    const login = document.getElementById('login').value.trim();
    const password = document.getElementById('password').value.trim();
    if (login && password && ws && ws.readyState === WebSocket.OPEN) {
        console.log('Attempting to register:', login);
        ws.send(`register:${login}:${password}`);
        username = login;
        // Автоматический логин после регистрации
        ws.onmessage = (event) => {
            console.log('Received message:', event.data);
            if (event.data === 'System: Registration successful') {
                console.log('Registration successful, attempting auto-login');
                ws.send(`login:${login}:${password}`);
            }
            addMessage(event.data);
            if (event.data.startsWith('System: Login successful')) {
                isLoggedIn = true;
                console.log('Login successful, setting isLoggedIn to true');
                document.getElementById('logoutButton').style.display = 'inline';
                document.getElementById('sendButton').disabled = false;
            }
            // Восстановить стандартный обработчик
            ws.onmessage = (event) => {
                console.log('Received message:', event.data);
                if (event.data.startsWith('System: Login successful')) {
                    isLoggedIn = true;
                    console.log('Login successful, setting isLoggedIn to true');
                    document.getElementById('logoutButton').style.display = 'inline';
                    document.getElementById('sendButton').disabled = false;
                } else if (event.data.startsWith('System: Logout successful')) {
                    isLoggedIn = false;
                    username = null;
                    console.log('Logout successful, resetting state');
                    document.getElementById('logoutButton').style.display = 'none';
                    document.getElementById('login').value = '';
                    document.getElementById('password').value = '';
                    document.getElementById('sendButton').disabled = true;
                    isConnecting = false; // Сбрасываем перед переподключением
                    setTimeout(connect, 1000);
                }
                addMessage(event.data);
            };
        };
    } else {
        console.error('Cannot register: invalid input or WebSocket not open');
        addMessage('System: Please enter login/password and ensure connection');
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            addMessage('System: Reconnecting...');
            isConnecting = false;
            setTimeout(connect, 1000);
        }
    }
}

function login() {
    const login = document.getElementById('login').value.trim();
    const password = document.getElementById('password').value.trim();
    if (login && password && ws && ws.readyState === WebSocket.OPEN) {
        console.log('Attempting to login:', login);
        ws.send(`login:${login}:${password}`);
        username = login;
    } else {
        console.error('Cannot login: invalid input or WebSocket not open');
        addMessage('System: Please enter login/password and ensure connection');
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            addMessage('System: Reconnecting...');
            isConnecting = false;
            setTimeout(connect, 1000);
        }
    }
}

function logout() {
    if (isLoggedIn && ws && ws.readyState === WebSocket.OPEN && username) {
        console.log('Attempting to logout:', username);
        ws.send(`logout:${username}`);
    } else {
        console.error('Cannot logout: not logged in or WebSocket not open');
        addMessage('System: Please login first');
    }
}

function sendMessage() {
    const message = document.getElementById('message').value.trim();
    if (message && ws && ws.readyState === WebSocket.OPEN && isLoggedIn && username) {
        console.log('Attempting to send message:', message);
        ws.send(`${username}: ${message}`);
        document.getElementById('message').value = '';
    } else {
        console.error('Cannot send: message empty, WebSocket not open, or no login');
        addMessage('System: Please login and enter a message');
        if (!ws || ws.readyState !== WebSocket.OPEN) {
            addMessage('System: Reconnecting...');
            isConnecting = false;
            setTimeout(connect, 1000);
        }
    }
}

function addMessage(message) {
    console.log('Adding message to DOM:', message);
    const messages = document.getElementById('messages');
    const li = document.createElement('li');
    li.textContent = message;
    messages.appendChild(li);
    messages.scrollTop = messages.scrollHeight;
}

document.getElementById('registerButton').addEventListener('click', register);
document.getElementById('loginButton').addEventListener('click', login);
document.getElementById('logoutButton').addEventListener('click', logout);
document.getElementById('sendButton').addEventListener('click', sendMessage);
document.getElementById('message').addEventListener('keypress', (e) => {
    if (e.key === 'Enter') sendMessage();
});

connect();