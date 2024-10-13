QT += gui core widgets multimedia

TARGET = voiceChanger
TEMPLATE = app

CONFIG += c++17

SOURCES += main.cpp

HEADERS +=

INCLUDEPATH += /Users/macbook2015/Desktop/brew/include/soundtouch


LIBS += -L/Users/macbook2015/Desktop/brew/lib -lsoundtouch

