# Web Client (C++)

Web-client модуль системы массового тестирования.
Web Client — C++ модуль системы массового тестирования. Это HTTP-прокси и UI-страница,
которая управляет пользовательскими сессиями, оркестрирует процесс входа и
интегрируется с модулем авторизации и основным Main Module.

## Роль
- Управление сессиями пользователей
- Редиректы и сценарии входа
- Взаимодействие с модулем авторизации
- Stateless архитектура + Redis
## Роль и возможности
- Управление пользовательскими сессиями (cookie + Redis).
- Редиректы и сценарии входа (OAuth и одноразовый код).
- Интеграция с Auth Module и Main Module.
- Stateless-архитектура, состояние хранится в Redis.

## Состояния пользователя
- UNKNOWN
- ANONYMOUS
- AUTHORIZED
- `UNKNOWN`
- `ANONYMOUS`
- `AUTHORIZED`

## Архитектура и ключевые компоненты
- **Crow** — HTTP сервер (маршрутизация, запросы/ответы).
- **Redis** — хранение сессий (`session:<uuid>`).
- **Auth Client** — вызовы в модуль авторизации (OAuth, статус, refresh).
- **Main Client** — проксирование запросов в Main Module с bearer-токеном.
- **HTML-страницы** — простой серверный HTML для login/dashboard.

## Запуск
```bash
docker-compose up --build
```

Если сборка падает с ошибкой `failed to execute bake: read |0: file already closed`,
выполните запуск с отключённым bake:

```bash
COMPOSE_BAKE=false docker-compose up --build
```

Если сборка падает с ошибкой CMake вида `Parse error ... got unquoted argument with text "--git"`,
проверьте, что в `CMakeLists.txt` не остались конфликтные маркеры или куски diff
(`diff --git`, `<<<<<<<`, `=======`, `>>>>>>>`) после merge.

## Конфигурация
Можно переопределить адреса модулей через переменные окружения:
Адреса внешних модулей можно переопределить через переменные окружения:

- `AUTH_URL` (по умолчанию `https://religiose-multinodular-jaqueline.ngrok-free.dev`)
- `MAIN_URL` (по умолчанию `https://shabbiest-continuately-zulma.ngrok-free.dev`)
- `MAIN_BASE_URL` (альтернатива `MAIN_URL`, имеет приоритет)

## Интеграция с модулем авторизации
## Интеграция с Auth Module
Web Client ожидает следующие эндпоинты:

- `POST /auth/oauth/start?provider=github|yandex&token_login=...` → ссылка для OAuth
- `POST /auth/code/start?token_login=...` → одноразовый код
- `GET /auth/status?token_login=...` → статус и JWT-токены
- `POST /auth/refresh` → refresh-пара токенов

## Интеграция с Main Module
Web Client проксирует запросы в Main Module (используя access token). Дополнительно
реализованы базовые страницы и маршруты:

- `/` → dashboard, загрузка курсов/уведомлений/пользователей
- `/courses`, `/course?course_id=...`
- `/users`, `/user?id=...`
- `/notifications`

## Пример настройки Main Module
Для подключения к внешнему Main Module используйте переменную `MAIN_BASE_URL` (или `MAIN_URL`), например:

```bash
export MAIN_BASE_URL="https://shabbiest-continuately-zulma.ngrok-free.dev"
```

## Отладка и логи
- Логи сервиса доступны через `docker-compose logs -f web`.
- Для Redis: `docker-compose logs -f redis`.

## Полезные файлы
- `src/main.cpp` — точка входа, регистрация маршрутов.
- `src/handlers/*.cpp` — основные маршруты и логика.
- `src/api/*.cpp` — HTTP-клиенты для Auth и Main.
- `src/redis.*` — клиент Redis (RESP).
- `docker-compose.yml`, `nginx/nginx.conf` — окружение и прокси.
