QT += core gui widgets sql network printsupport charts serialport

lessThan(QT_MAJOR_VERSION, 6): error("This project requires Qt 6.")

CONFIG += c++17
msvc: QMAKE_CXXFLAGS += /utf-8
else: QMAKE_CXXFLAGS += -finput-charset=UTF-8 -fexec-charset=UTF-8

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
