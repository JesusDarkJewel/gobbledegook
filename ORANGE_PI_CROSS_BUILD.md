# Кросс-сборка Gobbledegook для Orange Pi 3 LTS

Целевая плата в проверенной конфигурации:

- Orange Pi 3 LTS;
- Debian 11 (bullseye), `aarch64`;
- адрес `orangepi@192.168.128.54`;
- Bluetooth-контроллер `D8:00:72:E4:44:6B`;
- glibc 2.31, GCC runtime 10.

## 1. Подготовка Orange Pi

Отключить устаревший репозиторий `bullseye-backports`, если `apt update`
сообщает, что у него отсутствует Release-файл:

```bash
sudo sed -i \
  's|^deb http://mirrors.tuna.tsinghua.edu.cn/debian bullseye-backports|# disabled obsolete bullseye-backports: deb http://mirrors.tuna.tsinghua.edu.cn/debian bullseye-backports|' \
  /etc/apt/sources.list
```

Установить BlueZ и development-пакеты:

```bash
sudo apt-get update
sudo apt-get install -y \
  bluez libglib2.0-dev libdbus-1-dev libbluetooth-dev
sudo systemctl enable --now bluetooth.service
```

Проверка:

```bash
systemctl is-active bluetooth
bluetoothctl show
```

Контроллер должен иметь роли `central` и `peripheral`, а также ненулевое
число `SupportedInstances` в секции Advertising Features.

## 2. Windows AArch64 toolchain

Для Debian 11 следует использовать официальный Arm GNU Toolchain
10.3-2021.07:

```text
gcc-arm-10.3-2021.07-mingw-w64-i686-aarch64-none-linux-gnu.tar.xz
```

Проверенный URL:

```text
https://developer.arm.com/-/media/Files/downloads/gnu-a/10.3-2021.07/binrel/gcc-arm-10.3-2021.07-mingw-w64-i686-aarch64-none-linux-gnu.tar.xz
```

Распаковать содержимое без верхнего каталога в:

```text
%LOCALAPPDATA%\ArmGNUToolchain\10.3-2021.07-aarch64-linux
```

GCC 15.2 для этой платы не подходит: его `libstdc++` требует функции glibc
2.33/2.42, отсутствующие в Debian 11 с glibc 2.31.

## 3. Создание sysroot

Из PowerShell в корне проекта:

```powershell
$sysroot = ".sysroot-orange\usr"
New-Item -ItemType Directory -Force $sysroot
scp -r orangepi@192.168.128.54:/usr/include $sysroot
New-Item -ItemType Directory -Force "$sysroot\lib"
scp -r orangepi@192.168.128.54:/usr/lib/aarch64-linux-gnu "$sysroot\lib"
```

Sysroot содержит заголовки и библиотеки именно целевой платы. Его не следует
добавлять в Git.

## 4. Сборка

```powershell
powershell -ExecutionPolicy Bypass -File .vscode\build-orange.ps1
```

Результаты:

```text
libggk-orange.a
standalone-orange
```

Скрипт использует режим `-std=c++2a`, поскольку GCC 10 ещё не принимает
современное имя `-std=c++23`. Используемые проектом возможности языка
поддерживаются GCC 10.

`libstdc++.so.6`, `libgcc_s.so.1`, GLib и D-Bus при линковке берутся из
sysroot Orange Pi. Это удерживает требование бинарника в пределах GLIBC 2.30.

## 5. D-Bus policy

Установить `gobbledegook.conf`:

```bash
sudo install -o root -g root -m 0644 \
  gobbledegook.conf /etc/dbus-1/system.d/gobbledegook.conf
sudo systemctl reload dbus.service
```

Policy должна разрешать пользователю root владеть именем
`com.gobbledegook` и обращаться к `org.bluez`.

После обновления пакета D-Bus система может потребовать перезагрузку.

## 6. Загрузка и тест

```powershell
scp standalone-orange orangepi@192.168.128.54:/home/orangepi/
ssh orangepi@192.168.128.54 `
  "chmod +x /home/orangepi/standalone-orange"
```

Временный unit:

```bash
sudo systemd-run --unit=ggk-smoke --collect \
  /home/orangepi/standalone-orange -d
journalctl -u ggk-smoke.service -f
```

Остановка:

```bash
sudo systemctl stop ggk-smoke.service
```

## 7. Особенность Orange Pi 3 LTS

Bluetooth Management command `Set Local Name (0x000F)` зависает на текущем
драйвере платы. После команды HCI остаётся выключенным, а процесс может
застрять в kernel wait.

Поэтому sample `standalone.cpp` создаёт сервер с пустыми advertising name и
short name. Это предусмотренный Gobbledegook способ сохранить системное имя
адаптера и не отправлять несовместимую команду:

```cpp
std::string{},
std::string{},
```

Если старый бинарник уже завис на `Set Local Name`, обычный SIGKILL может не
помочь. Требуется перезагрузка или отключение питания. На текущем образе
мягкая перезагрузка после зависания HCI иногда не возвращает Wi-Fi; тогда
нужно передёрнуть питание.

## 8. Проверка рекламы с Windows

Из `C:\Projects\BLEBench`:

```powershell
.\build\Release\blebench.exe scan 30
```

После обнаружения адреса:

```powershell
.\build\Release\blebench.exe gatt AA:BB:CC:DD:EE:FF
```
