QT += core gui widgets sql network printsupport charts serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17
QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

SOURCES += \
    arduino.cpp \
    main.cpp \
    connexion.cpp \
    bateau.cpp \
    emplacement.cpp \
    quai.cpp \
    sortie.cpp \
    employe.cpp \
    fishingport.cpp

HEADERS += \
    arduino.h \
    connexion.h \
    bateau.h \
    emplacement.h \
    quai.h \
    sortie.h \
    employe.h \
    fishingport.h

FORMS += \
    fishingport.ui

RESOURCES += \
    resources.qrc

# Default rules for deployment
qnx: target.path = /tmp/${TARGET}/bin
else: unix:!android: target.path = /opt/${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
