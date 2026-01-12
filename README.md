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
