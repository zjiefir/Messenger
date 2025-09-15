const ws = new WebSocket('ws://localhost:8080');
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
};
function sendMessage() {
    const input = document.getElementById('messageInput');
    const message = input.value;
    console.log('Attempting to send message:', message);
    if (message && ws.readyState === WebSocket.OPEN) {
        console.log('Sending message:', message);
        ws.send(message);
        input.value = '';
    } else {
        console.log('Cannot send: message empty or WebSocket not open');
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