#!/bin/bash
ws="$1"
ws=$(echo "$ws" | sed -e 's/^C:/\/c/' -e 's/\\/\//g')

cd "$ws/src" || exit

files="DBusInterface.cpp DBusMethod.cpp DBusObject.cpp GattCharacteristic.cpp GattDescriptor.cpp GattInterface.cpp GattProperty.cpp GattService.cpp Gobbledegook.cpp HciAdapter.cpp HciSocket.cpp Init.cpp Logger.cpp Mgmt.cpp Server.cpp ServerUtils.cpp Utils.cpp"

for file in $files; do
    arm-linux-gnueabihf-g++ -c \
        -DHAVE_CONFIG_H \
        -fPIC -Wall -Wextra -std=c++11 \
        -I"$ws/includes/" \
        -I"$ws/includes/glib-2.0" \
        -I"$ws/includes/dbus-1.0/include" \
        -I"$ws/includes/glib-2.0/include" \
        -pthread \
        -g -O2 \
        "$file" -o "${file%.cpp}.o"
done

arm-linux-gnueabihf-ar cr ../libggk.a *.o
arm-linux-gnueabihf-ranlib ../libggk.a
rm -f *.o