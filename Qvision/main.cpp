#include "mainwindow.h"

#include <QApplication>
#include <QPixmap>
#include<QSplashScreen>

#include "DarkStyle.h"
#include "framelesswindow.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
   //设置暗色系皮肤
    QApplication::setStyle(new DarkStyle);
    QApplication::setPalette(QApplication::style()->standardPalette());

    //创建无边框界面窗口并将主窗口嵌入
    FramelessWindow framelessWindow;
    framelessWindow.resize(1100,600);
    QPixmap pixmap(":/image/cha.png");  //创建一个QPixmap对象,设置启动图片
    QSplashScreen splash(pixmap);  //利用QPixmap对象创建一个QSplashScreen对象
    splash.show();  //显示此启动图片

    framelessWindow.setWindowTitle("阿堂的图像魔法书");
    framelessWindow.setWindowIcon(QIcon(":/image/logo.ico"));
    QApplication::setWindowIcon(QIcon(":/image/logo.ico"));
    MainWindow *mainWindow = new MainWindow;
    framelessWindow.setContent(mainWindow);
    framelessWindow.show();

    return a.exec();
}

