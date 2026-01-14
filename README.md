# Web Client (C++)

Web-client модуль системы массового тестирования.

## Роль
- Управление сессиями пользователей
- Редиректы и сценарии входа
- Взаимодействие с модулем авторизации
- Stateless архитектура + Redis

## Состояния пользователя
- UNKNOWN
- ANONYMOUS
- AUTHORIZED

## Запуск
```bash
docker-compose up --build
```

Если сборка падает с ошибкой `failed to execute bake: read |0: file already closed`,
выполните запуск с отключённым bake:

```bash
COMPOSE_BAKE=false docker-compose up --build
```

## Конфигурация
Можно переопределить адреса модулей через переменные окружения:

- `AUTH_URL` (по умолчанию `http://auth:8081`)
- `MAIN_URL` (по умолчанию `http://main:8082`)
- `MAIN_BASE_URL` (альтернатива `MAIN_URL`, имеет приоритет)

## Интеграция с модулем авторизации
Web Client ожидает следующие эндпоинты:

- `POST /auth/oauth/start?provider=github|yandex&token_login=...` → ссылка для OAuth
- `POST /auth/code/start?token_login=...` → одноразовый код
- `GET /auth/status?token_login=...` → статус и JWT-токены

## Пример настройки Main Module
Для подключения к внешнему Main Module используйте переменную `MAIN_BASE_URL` (или `MAIN_URL`), например:

```bash
export MAIN_BASE_URL="https://shabbiest-continuately-zulma.ngrok-free.dev"
```
