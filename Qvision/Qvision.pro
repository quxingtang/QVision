QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    DarkStyle.cpp \
    convert.cpp \
    framelesswindow.cpp \
    main.cpp \
    mainwindow.cpp \
    windowdragger.cpp

HEADERS += \
    DarkStyle.h \
    convert.h \
    framelesswindow.h \
    mainwindow.h \ \
    windowdragger.h

FORMS += \
    framelesswindow.ui \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    darkstyle.qrc \
    framelesswindow.qrc \
    image.qrc

DISTFILES += \
    Qvision.pro.user

#OpenCV配置
INCLUDEPATH +=D:\OpenCV\build\include  \
              D:\OpenCV\build\include\opencv2

CONFIG(debug, debug|release): {

        LIBS += D:\OpenCV\build\x64\mingw\lib\libopencv_*d.dll.a
        }

        else:CONFIG(release, debug|release): {

        LIBS += D:\OpenCV\build\x64\mingw\lib\libopencv_*.dll.a

        }

