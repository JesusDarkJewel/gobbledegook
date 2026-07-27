# Настройка WSL для кросс-компиляции под Raspberry Pi Zero W

Эта инструкция описывает настройку Windows и WSL для сборки ARM-приложений под
оригинальный Raspberry Pi Zero W с 32-битной Raspberry Pi OS.

Проверенная конфигурация целевого устройства:

```text
Model: Raspberry Pi Zero W Rev 1.1
Hardware: BCM2835
Architecture: ARMv6
ABI: armhf (hard float)
FPU: VFPv2
OS: Raspbian GNU/Linux 13 (trixie)
```

> Важно: Debian понимает `armhf` как ARMv7, а Raspbian для Pi Zero собирает
> `armhf` под ARMv6. Поэтому стандартный Debian-компилятор
> `arm-linux-gnueabihf-g++` для этого устройства не подходит полностью, даже
> если передать ему `-march=armv6`: его `crt*.o`, `libgcc` и другие runtime-файлы
> всё равно собраны под ARMv7.

## 1. Проверка модели и архитектуры Raspberry Pi

На Raspberry Pi выполнить:

```bash
uname -a
getconf LONG_BIT
dpkg --print-architecture
cat /etc/os-release

grep -E '^(model name|CPU architecture|Hardware|Model|Revision)' /proc/cpuinfo
```

Для оригинального Pi Zero W ожидаются значения наподобие:

```text
armv6l
32
armhf
Hardware : BCM2835
Model    : Raspberry Pi Zero W Rev 1.1
```

Если устройство действительно является Zero 2 W и использует 64-битную ОС,
эта инструкция не подходит: для него нужен target `aarch64`.

## 2. Установка WSL с Debian

Открыть PowerShell от имени администратора:

```powershell
wsl --list --online
wsl --install -d Debian
```

Если Windows попросит перезагрузку — перезагрузить компьютер. При первом
запуске Debian создать Linux-пользователя и пароль.

Проверить состояние:

```powershell
wsl -l -v
```

Желательно использовать WSL 2:

```text
NAME      STATE    VERSION
Debian    Running  2
```

При необходимости переключить Debian на WSL 2:

```powershell
wsl --set-version Debian 2
```

## 3. Базовые пакеты WSL

В терминале Debian:

```bash
sudo apt update
sudo apt install build-essential git make rsync xz-utils
```

Стандартный Debian cross compiler можно установить для общих экспериментов:

```bash
sudo apt install g++-arm-linux-gnueabihf binutils-arm-linux-gnueabihf
```

Но не следует использовать его для итоговых бинарников Pi Zero W: он настроен
под ARMv7-A, Thumb-2 и VFPv3-D16.

Проверить это можно командой:

```bash
arm-linux-gnueabihf-g++ -v
```

В конфигурации Debian будет присутствовать:

```text
--with-arch=armv7-a+fp
--with-mode=thumb
```

## 4. Установка правильного ARMv6 toolchain

Используется готовый современный toolchain проекта `tttapa/toolchains`,
собранный посредством crosstool-NG:

```text
Target: armv6-rpi-linux-gnueabihf
GCC: 14.3
CPU tuning: arm1176jzf-s
Architecture: armv6+fp
FPU: vfp
Float ABI: hard
TLS: native
```

Страница проекта:

```text
https://github.com/tttapa/toolchains
```

### Вариант A: загрузка внутри WSL

Если в Debian установлен `wget`:

```bash
mkdir -p ~/opt

wget \
  https://github.com/tttapa/toolchains/releases/latest/download/x-tools-armv6-rpi-linux-gnueabihf-gcc14.tar.xz \
  -O /tmp/x-tools-armv6-rpi-linux-gnueabihf-gcc14.tar.xz

tar -xJf /tmp/x-tools-armv6-rpi-linux-gnueabihf-gcc14.tar.xz -C ~/opt
rm /tmp/x-tools-armv6-rpi-linux-gnueabihf-gcc14.tar.xz
```

Если `wget` отсутствует:

```bash
sudo apt install wget
```

### Вариант B: загрузка через Windows PowerShell

Если минимальная установка Debian не содержит ни `wget`, ни `curl`, выполнить
в PowerShell:

```powershell
curl.exe -fL --retry 3 `
  https://github.com/tttapa/toolchains/releases/latest/download/x-tools-armv6-rpi-linux-gnueabihf-gcc14.tar.xz `
  -o C:\Projects\gobbledegook\x-tools-armv6-rpi-linux-gnueabihf-gcc14.tar.xz
```

Затем распаковать из WSL:

```bash
mkdir -p ~/opt

tar -xJf \
  /mnt/c/Projects/gobbledegook/x-tools-armv6-rpi-linux-gnueabihf-gcc14.tar.xz \
  -C ~/opt
```

После успешной установки архив можно удалить.

## 5. Проверка toolchain

```bash
~/opt/x-tools/armv6-rpi-linux-gnueabihf/bin/armv6-rpi-linux-gnueabihf-g++ -v
```

Ожидаемые параметры:

```text
Target: armv6-rpi-linux-gnueabihf
--with-arch=armv6+fp
--with-tune=arm1176jzf-s
--with-fpu=vfp
--with-float=hard
Thread model: posix
```

При желании добавить toolchain в `PATH`:

```bash
echo \
  'export PATH="$HOME/opt/x-tools/armv6-rpi-linux-gnueabihf/bin:$PATH"' \
  >> ~/.profile

source ~/.profile
```

Проект Gobbledegook не требует изменения `PATH`: build-скрипт использует
абсолютный путь и позволяет переопределить его через `GGK_TOOLCHAIN_BIN`.

## 6. Smoke test для TLS и стандартной библиотеки C++

Создать `tls-smoke.cpp`:

```cpp
#include <mutex>

int main()
{
    static std::once_flag flag;
    std::call_once(flag, [] {});
}
```

Собрать:

```bash
TC="$HOME/opt/x-tools/armv6-rpi-linux-gnueabihf/bin/armv6-rpi-linux-gnueabihf"

"${TC}-g++" \
  -std=c++23 \
  -pthread \
  -no-pie \
  tls-smoke.cpp \
  -o tls-smoke-armv6
```

Проверить ABI:

```bash
"${TC}-readelf" -A tls-smoke-armv6
```

Ожидается:

```text
Tag_CPU_name: "6"
Tag_CPU_arch: v6
Tag_THUMB_ISA_use: Thumb-1
Tag_FP_arch: VFPv2
Tag_ABI_VFP_args: VFP registers
```

Проверить отсутствие несовместимого emulated TLS:

```bash
"${TC}-nm" tls-smoke-armv6 | grep '__emutls_v\._ZSt'
```

Команда не должна ничего вывести. Код `grep` должен быть `1`:

```bash
echo $?
```

Проверка на Raspberry Pi:

```powershell
scp C:\path\to\tls-smoke-armv6 jesus@10.0.0.2:~/BusOTS/
```

```bash
ssh jesus@10.0.0.2
cd ~/BusOTS
chmod +x tls-smoke-armv6
./tls-smoke-armv6
echo $?
```

Ожидаемый код завершения:

```text
0
```

## 7. Сборка Gobbledegook из VS Code

Проект может оставаться на Windows:

```text
C:\Projects\gobbledegook
```

Внутри WSL он доступен как:

```text
/mnt/c/Projects/gobbledegook
```

VS Code-задача `Build ggk (WSL)` определена в `.vscode/tasks.json` и запускает:

```text
wsl.exe --cd <workspaceFolder> bash .vscode/build.sh
```

Запуск вручную из PowerShell:

```powershell
wsl.exe --cd C:\Projects\gobbledegook bash .vscode/build.sh
```

Скрипт `.vscode/build.sh`:

1. Проверяет, что выполняется внутри Linux/WSL.
2. Проверяет наличие ARMv6 toolchain.
3. Собирает и проверяет TLS smoke test.
4. Компилирует исходники Gobbledegook.
5. Создаёт `libggk.a`.
6. Проверяет архив на `__emutls_v._ZSt*`.
7. Линкует `standalone-cross`.
8. Проверяет, что результат имеет `Tag_CPU_arch: v6`.

Артефакты:

```text
C:\Projects\gobbledegook\libggk.a
C:\Projects\gobbledegook\standalone-cross
C:\Projects\gobbledegook\tls-smoke-armv6
```

Переопределение расположения toolchain:

```bash
GGK_TOOLCHAIN_BIN=/другой/путь/bin \
  bash .vscode/build.sh
```

## 8. Почему при линковке используется `--allow-shlib-undefined`

В папке проекта `libs` находятся скопированные с Raspberry библиотеки:

```text
libgio-2.0.so
libgobject-2.0.so
libgmodule-2.0.so
libglib-2.0.so
libdbus-1.so
```

У них есть транзитивные зависимости:

```text
libz.so.1
libmount.so.1
libselinux.so.1
libffi.so.8
libpcre2-8.so.0
```

Эти библиотеки уже установлены на Raspberry Pi, но не скопированы в локальную
папку `libs`. Поэтому при линковке executable используется:

```text
-Wl,--allow-shlib-undefined
```

Этот параметр разрешает отложить разрешение зависимостей готовых `.so` до
запуска на Raspberry. Он не скрывает неопределённые символы из `libggk.a`.

Альтернативный строгий вариант — скопировать с Raspberry весь согласованный
sysroot и использовать `--sysroot`, но для текущего проекта это не потребовалось.

## 9. Ручная сборка основного проекта BusOTS

В WSL:

```bash
TC="$HOME/opt/x-tools/armv6-rpi-linux-gnueabihf/bin/armv6-rpi-linux-gnueabihf"

"${TC}-g++" \
  -g \
  -O2 \
  -std=c++23 \
  -pthread \
  -no-pie \
  -I/mnt/c/Projects/BusOTS_Experiments/includes \
  -I/mnt/c/Projects/BusOTS_Experiments/includes/glib-2.0 \
  -I/mnt/c/Projects/BusOTS_Experiments/includes/dbus-1.0/include \
  -I/mnt/c/Projects/BusOTS_Experiments/includes/glib-2.0/include \
  /mnt/c/Projects/BusOTS_Experiments/main.cpp \
  /mnt/c/Projects/gobbledegook/libggk.a \
  -L/mnt/c/Projects/BusOTS_Experiments/libs \
  -Wl,--allow-shlib-undefined \
  -o /mnt/c/Projects/gobbledegook/BusOTS-armv6-wsl-test \
  -l:libgio-2.0.so \
  -l:libgobject-2.0.so \
  -l:libgmodule-2.0.so \
  -l:libglib-2.0.so \
  -l:libdbus-1.so \
  -pthread
```

Проверить результат:

```bash
"${TC}-readelf" -A /mnt/c/Projects/gobbledegook/BusOTS-armv6-wsl-test

"${TC}-nm" /mnt/c/Projects/gobbledegook/BusOTS-armv6-wsl-test |
  grep '__emutls_v\._ZSt'
```

Должен быть ARMv6/VFPv2, а `grep` ничего не должен найти.

## 10. Копирование и запуск на Raspberry

Из PowerShell:

```powershell
scp `
  C:\Projects\gobbledegook\BusOTS-armv6-wsl-test `
  jesus@10.0.0.2:~/BusOTS/
```

На Raspberry:

```bash
cd ~/BusOTS
chmod +x BusOTS-armv6-wsl-test
sudo ./BusOTS-armv6-wsl-test
```

Проверенный результат:

```text
Starting GGK server...
The Bluetooth adapter is fully configured
GATT application registered with BlueZ
Server running.
```

## 11. Диагностика

### Segmentation fault до первого лога

Проверить архитектуру:

```bash
readelf -A ./BusOTS
```

Если вывод содержит:

```text
Tag_CPU_arch: v7
Tag_THUMB_ISA_use: Thumb-2
Tag_FP_arch: VFPv3-D16
```

бинарник был собран стандартным Debian ARMv7 toolchain и не подходит для
оригинального Pi Zero W.

Правильный вывод:

```text
Tag_CPU_arch: v6
Tag_THUMB_ISA_use: Thumb-1
Tag_FP_arch: VFPv2
```

### Ошибки `__emutls_v._ZSt15__once_callable`

Такой объект собран несовместимым Windows SysGCC, использующим emulated TLS:

```bash
arm-linux-gnueabihf-nm -A libggk.a |
  grep '__emutls_v\._ZSt'
```

Нужно пересобрать библиотеку ARMv6 toolchain из этой инструкции. Добавление
самодельных определений `__emutls_v.*` исправляет линковку, но не runtime:
заголовочный код и Raspberry `libstdc++.so` будут обращаться к разным TLS.

### Получение backtrace

На Raspberry:

```bash
sudo gdb ./BusOTS
run
thread apply all bt full
```

Неинтерактивный вариант:

```bash
sudo gdb -q -batch \
  -ex 'set pagination off' \
  -ex run \
  -ex 'thread apply all bt full' \
  --args ./BusOTS
```

### Проверка динамических библиотек

```bash
ldd ./BusOTS
readelf -d ./BusOTS
```

## 12. Итоговая схема

```text
VS Code / Windows
        |
        v
WSL 2 / Debian x86_64
        |
        v
armv6-rpi-linux-gnueabihf-g++ 14.3
        |
        +--> libggk.a
        |
        +--> standalone-cross
        |
        +--> BusOTS-armv6-wsl-test
                    |
                    v
        Raspberry Pi Zero W / ARMv6 / Raspbian armhf
```

Не использовать для итоговой сборки:

```text
C:\Users\user\AppData\Local\SysGCC
```

Он правильно ориентирован на ARMv6, но его Windows-hosted GCC некорректно
использует emulated TLS для внутренних объектов `libstdc++`.

Также не использовать стандартный Debian:

```text
/usr/bin/arm-linux-gnueabihf-g++
```

Он имеет корректный native TLS, но target runtime собран под ARMv7.
