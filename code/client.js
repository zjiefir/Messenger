let userName = '';

const ws = new WebSocket('ws://192.168.31.128:8080');
ws.onopen = () => {
    console.log('Connected to server!');
    addMessage('System: Connected to server');
};
ws.onmessage = (event) => {
    console.log('Received message:', event.data);
    addMessage(event.data);
};
ws.onerror = (error) => {
    console.error('WebSocket error:', error);
    addMessage('System: Error connecting to server');
};
ws.onclose = () => {
    addMessage('System: Disconnected from server');
    document.getElementById('nameForm').style.display = 'block';
    document.getElementById('chatForm').style.display = 'none';
};
function setUserName() {
    const input = document.getElementById('userName');
    userName = input.value.trim();
    if (userName) {
        console.log('Setting username:', userName);
        ws.send(`System: ${userName} joined the chat`);
        document.getElementById('nameForm').style.display = 'none';
        document.getElementById('chatForm').style.display = 'block';
        input.value = '';
    } else {
        alert('Please enter a name!');
    }
}
function sendMessage() {
    const input = document.getElementById('messageInput');
    const message = input.value.trim();
    console.log('Attempting to send message:', message);
    if (message && ws.readyState === WebSocket.OPEN && userName) {
        const fullMessage = `${userName}: ${message}`;
        console.log('Sending message:', fullMessage);
        ws.send(fullMessage);
        input.value = '';
    } else {
        console.log('Cannot send: message empty, WebSocket not open, or no username');
    }
}
function addMessage(message) {
    console.log('Adding message to DOM:', message);
    const messagesDiv = document.getElementById('messages');
    const p = document.createElement('p');
    p.textContent = message;
    messagesDiv.appendChild(p);
    messagesDiv.scrollTop = messagesDiv.scrollHeight;
}