QT += core gui widgets sql network printsupport charts serialport

lessThan(QT_MAJOR_VERSION, 6): error("This project requires Qt 6.")

CONFIG += c++17
TARGET = gestion_bateaux

INCLUDEPATH += $$PWD/src/app $$PWD/src/database $$PWD/src/models $$PWD/src/hardware
DEPENDPATH += $$INCLUDEPATH
msvc: QMAKE_CXXFLAGS += /utf-8
else: QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

SOURCES += \
    src/hardware/arduino.cpp \
    src/app/main.cpp \
    src/database/connexion.cpp \
    src/models/bateau.cpp \
    src/models/emplacement.cpp \
    src/models/quai.cpp \
    src/models/sortie.cpp \
    src/models/employe.cpp \
    src/app/fishingport.cpp

HEADERS += \
    src/hardware/arduino.h \
    src/database/connexion.h \
    src/models/bateau.h \
    src/models/emplacement.h \
    src/models/quai.h \
    src/models/sortie.h \
    src/models/employe.h \
    src/app/fishingport.h

FORMS += \
    ui/fishingport.ui

RESOURCES += \
    resources/resources.qrc

# Default rules for deployment
qnx: target.path = /tmp/${TARGET}/bin
else: unix:!android: target.path = /opt/${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
