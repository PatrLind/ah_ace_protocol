TEMPLATE = app
CONFIG += console
CONFIG += c++11
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CFLAGS_RELEASE = -Os -momit-leaf-frame-pointer
QMAKE_LFLAGS = -static -static-libgcc
DEFINES= QT_STATIC_BUILD

SOURCES += main.cpp

INCLUDEPATH += $$PWD/../../WpdPack/Include
DEPENDPATH += $$PWD/../../WpdPack/Include
LIBS += "-LC:/Dev/WpdPack/Lib/" -lwpcap -lws2_32

HEADERS +=
