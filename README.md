# Calendar (Этап 2 — SQLite)

## Сборка на Linux

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev libqt6sql6-sqlite

mkdir build && cd build
cmake ..
make -j$(nproc)
./QtCalendarApp
```

`libqt6sql6-sqlite` — это отдельный пакет с драйвером SQLite для Qt SQL
(на Debian/Ubuntu он не входит в `qt6-base-dev`). Без него `EventManager`
не сможет открыть базу — в консоли будет предупреждение вида
`QSqlDatabase: QSQLITE driver not loaded`.

Если `cmake ..` не находит Qt6 (например, Qt установлен через онлайн-инсталлятор
в нестандартный путь), укажите путь явно:

```bash
cmake .. -DCMAKE_PREFIX_PATH=/путь/до/Qt/6.x.x/gcc_64
```

## Где хранятся данные

База — файл `calendar.db` в стандартной папке данных приложения
(обычно `~/.local/share/QtCalendarApp/calendar.db` на Linux). Чтобы
начать «с чистого листа», можно просто удалить этот файл — при
следующем запуске он будет создан заново пустым.
