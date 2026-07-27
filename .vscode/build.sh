#!/bin/bash
set -euo pipefail

if [[ "$(uname -s)" != "Linux" ]]; then
    echo "error: this build must run inside WSL/Linux" >&2
    exit 1
fi

toolchain_bin="${GGK_TOOLCHAIN_BIN:-$HOME/opt/x-tools/armv6-rpi-linux-gnueabihf/bin}"
toolchain_prefix="$toolchain_bin/armv6-rpi-linux-gnueabihf"
cxx="${toolchain_prefix}-g++"
ar="${toolchain_prefix}-ar"
nm="${toolchain_prefix}-nm"
readelf="${toolchain_prefix}-readelf"

for tool in "$cxx" "$ar" "$nm" "$readelf"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo "error: $tool is not installed in WSL" >&2
        echo "expected the ARMv6 toolchain in: $toolchain_bin" >&2
        exit 1
    fi
done

ws="$(pwd -P)"
cd "$ws/src"

files=(
    DBusInterface.cpp
    DBusMethod.cpp
    DBusObject.cpp
    GattCharacteristic.cpp
    GattDescriptor.cpp
    GattInterface.cpp
    GattProperty.cpp
    GattService.cpp
    Gobbledegook.cpp
    HciAdapter.cpp
    HciSocket.cpp
    Init.cpp
    Logger.cpp
    Mgmt.cpp
    Server.cpp
    ServerUtils.cpp
    Utils.cpp
)
objects=()

cleanup() {
    if ((${#objects[@]})); then
        rm -f -- "${objects[@]}"
    fi
}
trap cleanup EXIT

"$cxx" --version | head -n 1

"$cxx" \
    -std=c++11 -pthread -no-pie \
    "$ws/.vscode/tls-smoke.cpp" \
    -o "$ws/tls-smoke-armv6"

if "$nm" "$ws/tls-smoke-armv6" |
    grep '__emutls_v\._ZSt' >/dev/null; then
    echo "error: toolchain emitted incompatible emulated TLS for libstdc++ symbols" >&2
    exit 1
fi

for file in "${files[@]}"; do
    object="${file%.cpp}.o"
    "$cxx" -c \
        -DHAVE_CONFIG_H \
        -fPIC -Wall -Wextra -std=c++11 \
        -I"$ws/includes/" \
        -I"$ws/includes/glib-2.0" \
        -I"$ws/includes/dbus-1.0/include" \
        -I"$ws/includes/glib-2.0/include" \
        -pthread \
        -g -O2 \
        "$file" -o "$object"
    objects+=("$object")
done

rm -f ../libggk.a
"$ar" rcs ../libggk.a "${objects[@]}"

if "$nm" ../libggk.a |
    grep '__emutls_v\._ZSt' >/dev/null; then
    echo "error: libggk.a contains incompatible emulated TLS for libstdc++ symbols" >&2
    exit 1
fi

"$cxx" \
    -g -O2 -std=c++11 -fPIC -pthread -no-pie \
    -DHAVE_CONFIG_H \
    -I"$ws/include" \
    -I"$ws/src" \
    -I"$ws/includes" \
    -I"$ws/includes/glib-2.0" \
    -I"$ws/includes/dbus-1.0/include" \
    -I"$ws/includes/glib-2.0/include" \
    "$ws/src/standalone.cpp" \
    -L"$ws" \
    -L"$ws/libs" \
    -Wl,--allow-shlib-undefined \
    -o "$ws/standalone-cross" \
    -lggk \
    -l:libgio-2.0.so \
    -l:libgobject-2.0.so \
    -l:libgmodule-2.0.so \
    -l:libglib-2.0.so \
    -l:libdbus-1.so \
    -pthread

"$readelf" -A "$ws/standalone-cross" |
    grep -q 'Tag_CPU_arch: v6'

echo "Built $ws/libggk.a"
echo "Linked $ws/standalone-cross"
