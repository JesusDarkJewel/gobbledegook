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
sudo DEBIAN_FRONTEND=noninteractive apt-get full-upgrade -y
sudo apt-get install -y \
  bluez libglib2.0-dev libdbus-1-dev libbluetooth-dev
sudo systemctl enable --now bluetooth.service
```

После `full-upgrade` следует перезагрузить плату и заново скопировать sysroot на
Windows: заголовки и библиотеки сборки должны точно соответствовать обновлённой
целевой системе. На проверенной Debian 11 установлены BlueZ
`5.55-3.1+deb11u2`, glibc `2.31-13+deb11u14` и systemd
`247.3-7+deb11u8`; это последние версии из штатных репозиториев bullseye.

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

Для Orange Pi библиотека собирается с:

```text
-DGGK_DISABLE_SET_LOCAL_NAME=1
```

Этот флаг отключает только изменение системного имени контроллера через
Bluetooth Management API. Значения `advertisingName` и
`advertisingShortName` сохраняются и по-прежнему используются для поля
Local Name в BLE-рекламе.

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
sudo btmgmt rm-adv 1
```

Последняя команда нужна только после аварийного завершения: transient unit
обычно удаляет рекламный instance сам. Если старый instance остался в ядре,
повторный `Add Advertising` вернёт `Busy (0x0A)`.

## 7. Особенность Orange Pi 3 LTS

Bluetooth Management command `Set Local Name (0x000F)` зависает на текущем
драйвере платы. После команды HCI остаётся выключенным, а процесс может
застрять в kernel wait.

Поэтому Orange Pi build использует compile-time capability flag:

```text
GGK_DISABLE_SET_LOCAL_NAME=1
```

В обычных сборках прежнее поведение сохранено. Кроме того, приложение может
явно вызвать `server->setEnableSetLocalName(false)` до `ggkRun()`.
Отключение команды не отключает имя маяка: `standalone` по-прежнему передаёт
`Gobbledegook` как full и short advertising name, а `Server::buildServices()`
добавляет full name в advertising data как AD type `0x09`.

`Mgmt::addAdvertising()` не включает kernel-managed Flags, TX Power или Local
Name. Приложение передаёт законченный AD buffer самостоятельно. Нельзя
одновременно передать собственные поля Flags/Local Name и запросить у kernel
добавление тех же полей: BlueZ MGMT возвращает `Invalid Parameters (0x0D)`.

Исключение — AD type `0x01 Flags`: `MGMT_OP_ADD_ADVERTISING` всегда формирует
его из command flags. Библиотека автоматически удаляет этот раздел из
пользовательского буфера, сохраняя Local Name, Service Data и Manufacturer
Specific Data. Это позволяет старому прикладному коду продолжать передавать
полный AD buffer.

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

Windows может показать GAP-имя контроллера (`orangepi3-lts`) после подключения
и не раскрыть raw AD sections для этого устройства. Фактическое имя в
рекламном пакете можно проверить на Orange Pi:

```bash
sudo btmon
```

В трассе `MGMT Command: Add Advertising` должно быть:

```text
Advertising data length: 14
Name (complete): Gobbledegook
```
