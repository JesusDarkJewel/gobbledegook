# Build a static library (libggk.a)
noinst_LIBRARIES = libggk.a
AM_CPPFLAGS = -I$(top_srcdir)/include/ggk
libggk_a_CXXFLAGS = -fPIC -Wall -Wextra -std=c++23 $(GLIB_CFLAGS) $(GIO_CFLAGS) $(GOBJECT_CFLAGS)
libggk_a_SOURCES = DBusInterface.cpp \
                   ../include/ggk/DBusInterface.h \
                   DBusMethod.cpp \
                   ../include/ggk/DBusMethod.h \
                   DBusObject.cpp \
                   ../include/ggk/DBusObject.h \
                   ../include/ggk/DBusObjectPath.h \
                   GattCharacteristic.cpp \
                   ../include/ggk/GattCharacteristic.h \
                   GattDescriptor.cpp \
                   ../include/ggk/GattDescriptor.h \
                   GattInterface.cpp \
                   ../include/ggk/GattInterface.h \
                   GattProperty.cpp \
                   ../include/ggk/GattProperty.h \
                   GattService.cpp \
                   ../include/ggk/GattService.h \
                   ../include/ggk/GattUuid.h \
                   ../include/ggk/Globals.h \
                   Gobbledegook.cpp \
                   ../include/ggk/Gobbledegook.h \
                   HciAdapter.cpp \
                   ../include/ggk/HciAdapter.h \
                   HciSocket.cpp \
                   ../include/ggk/HciSocket.h \
                   Init.cpp \
                   ../include/ggk/Init.h \
                   Logger.cpp \
                   ../include/ggk/Logger.h \
                   Mgmt.cpp \
                   ../include/ggk/Mgmt.h \
                   Server.cpp \
                   ../include/ggk/Server.h \
                   ServerUtils.cpp \
                   ../include/ggk/ServerUtils.h \
                   standalone.cpp \
                   ../include/ggk/TickEvent.h \
                   Utils.cpp \
                   ../include/ggk/Utils.h
# Build our standalone server (linking statically with libggk.a, linking dynamically with GLib)
standalone_CXXFLAGS = -fPIC -Wall -Wextra -std=c++23 $(GLIB_CFLAGS) $(GIO_CFLAGS) $(GOBJECT_CFLAGS)
noinst_PROGRAMS = standalone
standalone_SOURCES = standalone.cpp
standalone_LDADD = libggk.a
standalone_LDLIBS = $(GLIB_LIBS) $(GIO_LIBS) $(GOBJECT_LIBS)
