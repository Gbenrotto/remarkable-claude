TEMPLATE = lib
TARGET = ink-diary
CONFIG += shared plugin no_plugin_name_prefix c++17
QT += core gui qml quick svg network
SOURCES += src/diary.cpp xovi.cpp
QMAKE_CXXFLAGS += -fPIC
QMAKE_LFLAGS += -Wl,-Bsymbolic -Wl,--version-script=$$PWD/ink-diary.map

PRE_TARGETDEPS += $$PWD/ink-diary.map
