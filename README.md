# Calendar (MVP)

## Сборка на Linux

```bash
sudo apt update
sudo apt install build-essential cmake qt6-base-dev

mkdir build && cd build
cmake ..
make -j$(nproc)
./QtCalendarApp
```

Если `cmake ..` не находит Qt6 (например, Qt установлен в нестандартный
путь через онлайн-инсталлятор Qt), укажите путь явно:

```bash
cmake .. -DCMAKE_PREFIX_PATH=/путь/до/Qt/6.x.x/gcc_64
```
