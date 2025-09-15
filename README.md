## Messenger

Чат на WebSocket с сервером (C++, Boost.Beast) и клиентом (HTML, JS).

### Структура
- `code/server.cpp`: WebSocket-сервер (порт 8080).
- `code/index.html`: Интерфейс чата.
- `code/client.js`: WebSocket-клиент.
- `docs/`: Документация (`server.md`, `client.md`).

### Запуск
1. Сервер: Visual Studio, F5 (порт 8080).
2. Клиент:
   - В VSCode: Открой `code/index.html`, выбери "Open with Live Server" (порт 5500).
   - Или: `cd code; http-server -p 8000`, затем `http://localhost:8000`.
3. Тест: Открой две вкладки, отправь сообщение — оно появится в обеих.

### Статус
MVP готов! Сообщения отправляются и отображаются в реальном времени.
