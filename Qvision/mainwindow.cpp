#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "convert.h"
#include <QToolBar>
#include <QMessageBox>
#include <QDebug>
#include <QFileDialog>
#include <QTextCodec>
#include <QFileInfo>
#include <QFile>
#include <QListView>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include <QSlider>
#include <QSpinBox>
#include <QLineEdit>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QGridLayout>
#include <QMetaEnum>
#include <QDockWidget>
#include <QList>
#include <QClipboard>
#include <QDialog>
#include <opencv2/xfeatures2d/nonfree.hpp>
#include <opencv2/calib3d.hpp>
#include <opencv2/ximgproc.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/objdetect.hpp>
#include <opencv2/photo.hpp>
#include <vector>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    QPalette palette;
        setAutoFillBackground(true);
        setPalette(palette);

        control_widget = new QDockWidget(QString("控制部件"),this);
        control_widget->setStyleSheet("background-color:#FFFFFF;");
        addDockWidget(Qt::TopDockWidgetArea,control_widget);//窗口上方
        center_widget_in_dock = new QWidget;
        control_widget->setWidget(center_widget_in_dock);
        control_widget->hide();

        setAcceptDrops(true);//窗口可接收拖放事件

        action = MainWindow::selection::nothing;
        point_num = 0;

}

MainWindow::~MainWindow()
{
    delete ui;
}
//拖动事件 拖动打开图片
void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    //文件名以xx结尾
    QString imgName = event->mimeData()->urls()[0].fileName();
    if(!imgName.right(3).compare("jpg")||
       !imgName.right(3).compare("JPG")||
       !imgName.right(3).compare("png")||
       !imgName.right(3).compare("PNG")||
       !imgName.right(3).compare("bmp")||
       !imgName.right(3).compare("BMP")||
       !imgName.right(4).compare("jpeg")||
       !imgName.right(4).compare("JPEG"))
    {
        event->acceptProposedAction();
    }
    else
    {
        event->ignore();//不是这几种类型的图片不接受此事件
    }
}

//放下事件
void MainWindow::dropEvent(QDropEvent* event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if(urls.isEmpty())
    {
        return;
    }

    //打开图片
    src_path = urls.first().toLocalFile();

    if(src_path.isEmpty())
    {
        return;
    }
    clearLayout();

    QFileInfo filetemp = QFileInfo(src_path);
    imgfilename = filetemp.baseName();
    openfilepath = filetemp.path();
    savefilepath.clear();

    //读取文件
    srcImage = cv::imread(QTextCodec::codecForName("gb18030")->fromUnicode(src_path).data());

    //将BGR转换为RGB，方便操作习惯
    cv::cvtColor(srcImage,srcImage,cv::COLOR_BGR2RGB);
    img = QImage((const unsigned char*)(srcImage.data),
                 srcImage.cols,
                 srcImage.rows,
                 srcImage.cols*srcImage.channels(),
                 QImage::Format_RGB888);
    ui->label_src1->clear();
    ui->label_src2->clear();
    ui->label_dst->clear();
    ui->label_src1->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src1)));
    ui->label_dst->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_dst)));
    save_num = 1;
    point_num = 0;
    point_list.clear();
}

 //清除布局中的全部控件，清除后控件在界面上消失
void MainWindow::clearLayout()
{
 if(grildlayout)
     {
         QLayoutItem *child;
         while ((child = grildlayout->takeAt(0)) != nullptr)
         {
             if(child->widget())
             {
                 child->widget()->setParent(nullptr);//setParent为NULL，防止删除之后界面不消失
             }
             delete child;
         }

         delete grildlayout;
         grildlayout = nullptr;
     }

     //ui->labelsrc->action = my_label::Type::None;//src操作模式改成点选模式
     action = MainWindow::selection::nothing;
     point_list.clear();

     if(!srcImage.empty())
     {
         img = QImage((const unsigned char*)(srcImage.data),
                      srcImage.cols,
                      srcImage.rows,
                      srcImage.cols*srcImage.channels(),
                      QImage::Format_RGB888);
         ui->label_src1->clear();
         img = img.scaled(ui->label_src1->width(), ui->label_src1->height());
         ui->label_src1->setPixmap(QPixmap::fromImage(img));
     }

     if(is_continuity_process == true)//是否连续处理图像，是则将当前的效果图作为下次处理的源图
     {
         if(!dstImage.empty())
         {
             if(dstImage.type() == CV_8UC1)//如果处理后得到的是单通道图像直接复制给srcImage，进行其他操作可能会出错
             {
                 //将单通道的内容复制到3个通道
                 cv::Mat three_channel = cv::Mat::zeros(dstImage.rows,dstImage.cols,CV_8UC3);
                 std::vector<cv::Mat> channels;
                 for (int i = 0;i < 3;++i)
                 {
                     channels.push_back(dstImage);
                 }
                 merge(channels,three_channel);
                 three_channel.copyTo(srcImage);
             }
             else
             {
                 dstImage.copyTo(srcImage);
             }
             dstImage.release();
             img = QImage((const unsigned char*)(srcImage.data),
                          srcImage.cols,
                          srcImage.rows,
                          srcImage.cols*srcImage.channels(),
                          QImage::Format_RGB888);
             ui->label_src2->clear();
             ui->label_dst->clear();
             img = img.scaled(ui->label_src2->width(), ui->label_src2->height());
             ui->label_src2->setPixmap(QPixmap::fromImage(img));
         }
     }
}

//图片大小与label大小相适应
QImage MainWindow::ImageSetSize (QImage  qimage,QLabel *qLabel)
{
    QImage image;
    QSize imageSize = qimage.size();
    QSize labelSize = qLabel->size();

    double dWidthRatio = 1.0*imageSize.width() / labelSize.width();
    double dHeightRatio = 1.0*imageSize.height() / labelSize.height();
    if (dWidthRatio>dHeightRatio)
    {
        image = qimage.scaledToWidth(labelSize.width());
    }
    else
    {
        image = qimage.scaledToHeight(labelSize.height());
    }
    return image;

}

// 输出图片显示
void MainWindow::dstlabel_show(const cv::Mat & srcImage_temp, const cv::Mat & dstImage_temp)
  {
      img = QImage((const unsigned char*)(srcImage_temp.data),
                 srcImage_temp.cols,
                 srcImage_temp.rows,
                 srcImage_temp.cols * srcImage_temp.channels(),
                 QImage::Format_RGB888);
      img2 = QImage((const unsigned char*)(dstImage_temp.data),
                   dstImage_temp.cols,
                   dstImage_temp.rows,
                   dstImage_temp.cols * dstImage_temp.channels(),
                   QImage::Format_RGB888);
      ui->label_src1->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src2)));
      //ui->label_src2->setPixmap(QPixmap::fromImage(ImageSetSize(img2, ui->label_src2)));
      ui->label_dst->setPixmap(QPixmap::fromImage(ImageSetSize(img2, ui->label_dst)));

  }

// 打开图片
void MainWindow::on_action_open_triggered()
{
    src_path = QFileDialog::getOpenFileName(this,
                                            QString("打开图片文件"),
                                            openfilepath.isNull() ? "/" : openfilepath, //QDir::currentPath()
                                            QString("Image File(*.bmp *.jpg *.jpeg *.png)"));

    if(src_path.isEmpty())//比如打开选择对话框了随即点取消 fileName为空
    {
        qDebug()<<"图片路径为空";
        return;
    }

    clearLayout();

    QFileInfo filetemp = QFileInfo(src_path);
    imgfilename = filetemp.baseName();
    openfilepath = filetemp.path();
    savefilepath.clear();

    //读取文件
    srcImage = cv::imread(QTextCodec::codecForName("gb18030")->fromUnicode(src_path).data());
    dstImage.release();

    //将BGR转换为RGB，方便操作习惯
    cv::cvtColor(srcImage,srcImage,cv::COLOR_BGR2RGB);
    img = QImage((const unsigned char*)(srcImage.data),
                 srcImage.cols,
                 srcImage.rows,
                 srcImage.cols*srcImage.channels(),
                 QImage::Format_RGB888);
    ui->label_src1->clear();
    ui->label_src2->clear();
    ui->label_dst->clear();

    //显示图像到QLabel控件
    ui->label_src1->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src1)));
    ui->label_dst->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_dst)));

    save_num = 1;
    point_num = 0;
    point_list.clear();
}

//标签上显示二值图
void MainWindow::dstlabel_indexed8_show(const cv::Mat & dstImage_temp)
{
    //图片格式 Forcv::Mat_Indexed8
    cv::Mat dsttemp;
    //QImage2cvMat(dsttemp,dstImage_temp);
    dstImage_temp.copyTo(dsttemp);//二值图只有改了大小才能显示 中间变量用于输出效果 转为标签的大小
    cv::resize(dsttemp,dsttemp,cv::Size(ui->label_dst->width(), ui->label_dst->height()),0,0,3);

    img = QImage((const unsigned char*)(dsttemp.data),
                 dsttemp.cols,
                 dsttemp.rows,
                 dsttemp.cols * dsttemp.channels(),
                 QImage::Format_Indexed8);

    ui->label_dst->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_dst)));
}

// 保存效果图
void MainWindow::on_action_save_triggered()
{
    if (srcImage.empty()||dstImage.empty())
       {
           return;
       }

       QString saveFileName = QFileDialog::getSaveFileName(this,
                                                           QString("保存文件"),
                                                           QString(savefilepath.isNull() ? "/" : savefilepath) + "/" + imgfilename + QString("-效果图-%1.png").arg(save_num),
                                                           QString("image(*.png);;image(*.jpg);;image(*.jpeg);;image(*.bmp)"));
       if(saveFileName.isEmpty())//比如打开选择对话框了随即点取消 saveFileName为空
       {
           return;
       }

       std::string fileAsSave = QTextCodec::codecForName("gb18030")->fromUnicode(saveFileName).data();

       QFileInfo filetemp = QFileInfo(saveFileName);
       savefilepath = filetemp.path();
       //格式不同 保存方法不同
       if(dstImage.type() == CV_8UC3)//三通道图
       {
           cv::cvtColor(dstImage,dstImage,cv::COLOR_BGR2RGB);//保存所见的的/保存实际的
           imwrite(fileAsSave,dstImage);
       }
       else if(dstImage.type() == CV_8UC1)//二值化图
       {
           std::vector<int> compression_params;
           compression_params.push_back(cv::IMWRITE_PNG_COMPRESSION);
           imwrite(fileAsSave,dstImage,compression_params);
       }
    save_num++;
}

//清除图片
void MainWindow::on_action_clear_triggered()
{
    ui->label_src1->clear();
    ui->label_src2->clear();
    ui->label_dst->clear();
    srcImage.release();
    dstImage.release();
    clearLayout();
}

//还原图像
void MainWindow::on_action_back_triggered()
{
    if (src_path.isEmpty())
    {
        return;
    }

    srcImage = cv::imread(QTextCodec::codecForName("gb18030")->fromUnicode(src_path).data());

    cv::cvtColor(srcImage,srcImage,cv::COLOR_BGR2RGB);
    dstImage.release();
    img = QImage((const unsigned char*)(srcImage.data),
                 srcImage.cols,
                 srcImage.rows,
                 srcImage.cols*srcImage.channels(),
                 QImage::Format_RGB888);
    ui->label_src1->clear();
    ui->label_src2->clear();
    ui->label_dst->clear();
    ui->label_src1->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src1)));
    ui->label_src2->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src2)));
    ui->label_dst->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_dst)));

}

//退出
void MainWindow::on_action_exit_triggered()
{
    qApp->exit(0);
}

//创建一个 QLabel 控件，并将其添加到布局中
void MainWindow::grildLayoutAddWidgetLable(QString str_tem, int x, int y)
{
    QLabel* label = new QLabel(str_tem);
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::black);  // 设置文本颜色为黑
    label->setPalette(palette);
    grildlayout->addWidget(label, x, y);
}

//选择操作参数
void MainWindow::on_action_select_scope_triggered()
{
    if (srcImage.empty())
    {
        qDebug()<<"图片路径为空";
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * element_type = new QComboBox(center_widget_in_dock);
    element_type->addItem(QString("矩形"),cv::MORPH_RECT);
    element_type->addItem(QString("椭圆形"),cv::MORPH_ELLIPSE);
    element_type->addItem(QString("十字形"),cv::MORPH_CROSS);
    element_type->setView(new QListView());

    QComboBox * element_size_w = new QComboBox(center_widget_in_dock);
    element_size_w->setView(new QListView());
    for(int i = 1;i <= 30;i += 2)
    {
        element_size_w->addItem(QString("核宽：%1").arg(i),i);
    }

    QComboBox * element_size_h = new QComboBox(center_widget_in_dock);
    element_size_h->setView(new QListView());
    for(int i = 1;i <= 30;i += 2)
    {
        element_size_h->addItem(QString("核高：%1").arg(i),i);
    }

    QComboBox *morphfun = new QComboBox(center_widget_in_dock);
    morphfun->addItem(QString("腐蚀"),cv::MORPH_ERODE);
    morphfun->addItem(QString("膨胀"),cv::MORPH_DILATE);
    morphfun->addItem(QString("开运算"),cv::MORPH_OPEN);
    morphfun->addItem(QString("闭运算"),cv::MORPH_CLOSE);
    morphfun->addItem(QString("形态学梯度"),cv::MORPH_GRADIENT);
    morphfun->addItem(QString("顶帽"),cv::MORPH_TOPHAT);
    morphfun->addItem(QString("黑帽"),cv::MORPH_BLACKHAT);
    morphfun->setView(new QListView());

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("形态学处理-- 操作 核类型：",0,0);
    grildlayout->addWidget(element_type,0,1);
    grildLayoutAddWidgetLable(" 操作核尺寸：",0,2);
    grildlayout->addWidget(element_size_w,0,3);
    grildlayout->addWidget(element_size_h,0,4);
    grildLayoutAddWidgetLable(" 形态学操作类型：",0,5);
    grildlayout->addWidget(morphfun,0,6);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        cv::Mat element = getStructuringElement(element_type->currentData(Qt::UserRole).toInt(),
                                                cv::Size(element_size_w->currentData(Qt::UserRole).toInt(),
                                                         element_size_h->currentData(Qt::UserRole).toInt()),
                                                cv::Point(-1,-1));
        morphologyEx(srcImage,dstImage,morphfun->currentData(Qt::UserRole).toInt(),element);
        dstlabel_show(srcImage, dstImage);
    };
    morphfun->setStyleSheet("background-color:black;color:white");
    connect(morphfun,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    element_size_h->setStyleSheet("background-color:black;color:white");
    connect(element_size_h,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    element_size_w->setStyleSheet("background-color:black;color:white");
    connect(element_size_w,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    element_type->setStyleSheet("background-color:black;color:white");
    connect(element_type,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    fun();

}

//均值滤波
void MainWindow::on_action_bulr_triggered()
{
    if (srcImage.empty())
    {
        qDebug()<<"图片路径为空";
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * blursize_w = new QComboBox(center_widget_in_dock);
    blursize_w->setView(new QListView());
    for(int i = 1;i <= 20;i += 2)
    {
        blursize_w->addItem(QString("核宽：%1").arg(i),i);
    }

    QComboBox * blursize_h = new QComboBox(center_widget_in_dock);
    blursize_h->setView(new QListView());
    for(int i = 1;i <= 20;i += 2)
    {
        blursize_h->addItem(QString("核高：%1").arg(i),i);
    }

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("均值滤波--操作核尺寸：",0,0);
    grildlayout->addWidget(blursize_w,0,1);
    grildlayout->addWidget(blursize_h,0,2);
    grildlayout->setContentsMargins(20,20,20,20);

    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        cv::blur(srcImage,dstImage,
                 cv::Size(blursize_w->currentData(Qt::UserRole).toInt(),
                          blursize_h->currentData(Qt::UserRole).toInt()),
                 cv::Point(-1,-1));
        dstlabel_show(srcImage, dstImage);
    };
    blursize_w->setStyleSheet("background-color:black;color:white");
    connect(blursize_w,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    blursize_h->setStyleSheet("background-color:black;color:white");
    connect(blursize_h,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    fun();

}

//方框滤波
void MainWindow::on_action_boxFilter_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * boxFiltersize_w = new QComboBox(center_widget_in_dock);
    boxFiltersize_w->setView(new QListView());
    for(int i = 1;i <= 20;i += 2)
    {
        boxFiltersize_w->addItem(QString("核宽：%1").arg(i),i);
    }

    QComboBox * boxFiltersize_h = new QComboBox(center_widget_in_dock);
    boxFiltersize_h->setView(new QListView());
    for(int i = 1;i <= 20;i += 2)
    {
        boxFiltersize_h->addItem(QString("核高：%1").arg(i),i);
    }

    QCheckBox * isNormalize = new QCheckBox(center_widget_in_dock);
    isNormalize->setText(QString("进行归一化处理"));
    isNormalize->setChecked(true);//选中
    isNormalize->setFixedHeight(36);

    boxFiltersize_w->setFixedHeight(36);
    boxFiltersize_h->setFixedHeight(36);
    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("方框滤波--操作核尺寸：",0,0);
    grildlayout->addWidget(boxFiltersize_w,0,1);
    grildlayout->addWidget(boxFiltersize_h,0,2);
    grildlayout->addWidget(isNormalize,0,3);
    grildlayout->setContentsMargins(20,20,20,20);

    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        boxFilter(srcImage,dstImage,-1,
                  cv::Size(boxFiltersize_w->currentData(Qt::UserRole).toInt(),boxFiltersize_h->currentData(Qt::UserRole).toInt()),
                  cv::Point(-1,-1),isNormalize->isChecked());
        dstlabel_show(srcImage, dstImage);
    };
    boxFiltersize_w->setStyleSheet("background-color:black;color:white");
    connect(boxFiltersize_w,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    boxFiltersize_h->setStyleSheet("background-color:black;color:white");
    connect(boxFiltersize_h,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    connect(isNormalize,&QCheckBox::clicked,fun);
    fun();

}

//高斯滤波
void MainWindow::on_action_GaussFilter_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
        clearLayout();

        if(control_widget->isHidden())
        {
            control_widget->show();
        }

    QComboBox * gausssize_w = new QComboBox(center_widget_in_dock);
    gausssize_w->setView(new QListView());
    for(int i = 1;i <= 20;i += 2)
    {
        gausssize_w->addItem(QString("核宽：%1").arg(i),i);
    }

    QComboBox * gausssize_h = new QComboBox(center_widget_in_dock);
    gausssize_h->setView(new QListView());
    for(int i = 1;i <= 20;i += 2)
    {
        gausssize_h->addItem(QString("核高：%1").arg(i),i);
    }

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("高斯滤波--操作核尺寸：",0,0);
    grildlayout->addWidget(gausssize_w,0,1);
    grildlayout->addWidget(gausssize_h,0,2);
    grildlayout->setContentsMargins(20,20,20,20);

    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        cv::GaussianBlur(srcImage,dstImage,cv::Size(gausssize_w->currentData(Qt::UserRole).toInt(),
                                                    gausssize_h->currentData(Qt::UserRole).toInt()),0,0);
        dstlabel_show(srcImage,dstImage);
    };
    gausssize_w->setStyleSheet("background-color:black;color:white");
    connect(gausssize_w,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    gausssize_h->setStyleSheet("background-color:black;color:white");
    connect(gausssize_h,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    fun();
}

//中值滤波
void MainWindow::on_action_midian_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * mediansize = new QComboBox(center_widget_in_dock);
    mediansize->setView(new QListView());
    for(int i = 3;i <= 20;i += 2)
    {
        mediansize->addItem(QString("%1 × %1").arg(i),i);
    }

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("中值滤波--操作核尺寸：",0,0);
    grildlayout->addWidget(mediansize,0,1);
    grildlayout->setContentsMargins(20,20,20,20);

    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        medianBlur(srcImage,dstImage,mediansize->currentData(Qt::UserRole).toInt());
        dstlabel_show(srcImage,dstImage);
    };
    mediansize->setStyleSheet("background-color:black;color:white");
    connect(mediansize,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    fun();
}

// 双边滤波
void MainWindow::on_action_bilateral_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
        {
            control_widget->show();
        }
    QSpinBox * colorsigma_SpinBox = new QSpinBox(center_widget_in_dock);
    colorsigma_SpinBox->setMinimum(1);
    colorsigma_SpinBox->setMaximum(254);
    colorsigma_SpinBox->setValue(1);
    colorsigma_SpinBox->setFixedHeight(36);
    colorsigma_SpinBox->setStyleSheet("background:black;color:white");

    QSlider * colorsigma_slider = new QSlider(center_widget_in_dock);
    colorsigma_slider->setOrientation(Qt::Horizontal);
    colorsigma_slider->setMinimum(1);
    colorsigma_slider->setMaximum(254);
    colorsigma_slider->setSingleStep(10);
    colorsigma_slider->setTickInterval(15);
    colorsigma_slider->setTickPosition(QSlider::TicksAbove);
    colorsigma_slider->setValue(1);
    colorsigma_slider->setFixedHeight(36);
    colorsigma_slider->setStyleSheet("background-color:black;color:white");

    QSpinBox * spacesigma_SpinBox = new QSpinBox(center_widget_in_dock);
    spacesigma_SpinBox->setMinimum(3);
    spacesigma_SpinBox->setMaximum(100);
    spacesigma_SpinBox->setValue(3);
    spacesigma_SpinBox->setFixedHeight(36);

    QSlider * spacesigma_slider = new QSlider(center_widget_in_dock);
    spacesigma_slider->setOrientation(Qt::Horizontal);
    spacesigma_slider->setMinimum(3);
    spacesigma_slider->setMaximum(100);
    spacesigma_slider->setSingleStep(10);
    spacesigma_slider->setTickInterval(15);
    spacesigma_slider->setTickPosition(QSlider::TicksAbove);
    spacesigma_slider->setValue(3);
    spacesigma_slider->setFixedHeight(36);

    QPushButton * is_chose_bilateral = new QPushButton("确定",center_widget_in_dock);
    is_chose_bilateral->setStyleSheet(QString("QPushButton{color:white;border-radius:5px;background:black;font-size:22px}"
                                              "QPushButton:hover{background:black;}"
                                              "QPushButton:pressed{background:black;}"));
                                      //.arg(colorMain).arg(colorHover).arg(colorPressed));
    is_chose_bilateral->setFixedHeight(36);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("颜色范围标准差，范围：1~254：",0,0);
    grildlayout->addWidget(colorsigma_SpinBox,0,1);
    grildlayout->addWidget(colorsigma_slider,0,2);
    grildLayoutAddWidgetLable("注意：空间距离标准差值较大时会很耗时",0,3);
    grildLayoutAddWidgetLable("空间距离标准差，范围：3~100：",1,0);
    grildlayout->addWidget(spacesigma_SpinBox,1,1);
    grildlayout->addWidget(spacesigma_slider,1,2);
    grildlayout->addWidget(is_chose_bilateral,1,3);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    spacesigma_SpinBox->setStyleSheet("background-color:black;color:white");
    spacesigma_slider->setStyleSheet("background-color:black;color:white");
    connect(spacesigma_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), spacesigma_slider, &QAbstractSlider::setValue);
    connect(spacesigma_slider, &QAbstractSlider::valueChanged, spacesigma_SpinBox, &QSpinBox::setValue);
    spacesigma_SpinBox->setStyleSheet("background-color:black;color:white");
    spacesigma_slider->setStyleSheet("background-color:black;color:white");
    connect(colorsigma_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), colorsigma_slider, &QAbstractSlider::setValue);
    connect(colorsigma_slider, &QAbstractSlider::valueChanged, colorsigma_SpinBox, &QSpinBox::setValue);

    connect(is_chose_bilateral, &QAbstractButton::clicked,[=]
    {
        bilateralFilter(srcImage,dstImage,-1,colorsigma_SpinBox->text().toInt(),spacesigma_SpinBox->text().toInt());
        dstlabel_show(srcImage,dstImage);
    });
}

//导向滤波
void MainWindow::on_actionGuidedFilter_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
        clearLayout();

        if(control_widget->isHidden())
        {
            control_widget->show();
        }

        QSpinBox * spinbox = new QSpinBox(center_widget_in_dock);
        spinbox->setMinimum(1);
        spinbox->setMaximum(15000);
        spinbox->setValue(1);
        spinbox->setFixedHeight(36);

        QSlider * slider = new QSlider(center_widget_in_dock);
        slider->setOrientation(Qt::Horizontal);
        slider->setMinimum(1);
        slider->setMaximum(15000);
        slider->setSingleStep(1000);
        slider->setTickInterval(600);
        slider->setTickPosition(QSlider::TicksAbove);
        slider->setValue(1);
        slider->setFixedHeight(36);

        QComboBox * combox = new QComboBox(center_widget_in_dock);
        combox->setView(new QListView());
        combox->setStyleSheet("background-color:black;color:white");
        for(int i = 3;i <= 40;i += 2)
        {
            combox->addItem(QString("%1").arg(i),i);
        }

        grildlayout = new QGridLayout(center_widget_in_dock);
        grildLayoutAddWidgetLable("卷积核尺寸：",0,0);
        grildlayout->addWidget(combox,0,1);
        grildLayoutAddWidgetLable("规范化参数：",0,2);
        grildlayout->addWidget(spinbox,0,3);
        grildlayout->addWidget(slider,0,4);
        grildlayout->setContentsMargins(20,20,20,20);
        grildlayout->setAlignment(Qt::AlignHCenter);
        center_widget_in_dock->setLayout(grildlayout);

        auto fun = [=]
        {
            cv::ximgproc::guidedFilter(srcImage,srcImage,dstImage,combox->currentData().toInt(),spinbox->value(),-1);
            dstlabel_show(srcImage,dstImage);
        };
        spinbox->setStyleSheet("background-color:black;color:white");
        slider->setStyleSheet("background-color:black;color:white");
        connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged), slider, &QAbstractSlider::setValue);
        connect(slider, &QAbstractSlider::valueChanged, spinbox, &QSpinBox::setValue);
        connect(spinbox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
        connect(slider, &QAbstractSlider::valueChanged, fun);
        connect(combox,QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
        fun();
}

// 直方图均值化
void MainWindow::on_action_equalizehist_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(!control_widget->isHidden())
    {
        control_widget->hide();
    }

    std::vector<cv::Mat> splitBGR;
    split(srcImage,splitBGR);
    for(int i = 0;i < srcImage.channels();++i)
    {
        equalizeHist(splitBGR[i],splitBGR[i]);
    }
    merge(splitBGR,dstImage);
    dstlabel_show(srcImage,dstImage);
}

//直方图（线状）
void MainWindow::on_action_hist_line_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(!control_widget->isHidden())
    {
        control_widget->hide();
    }

    std::vector<cv::Mat> bgr_planes;
    split(srcImage, bgr_planes);

    //计算直方图
    int histsize = 256;
    float range[] = { 0,256 };
    const float*histRanges = { range };
    cv::Mat b_hist, g_hist, r_hist;
    calcHist(&bgr_planes[0], 1, nullptr, cv::Mat(), b_hist, 1, &histsize, &histRanges, true, false);
    calcHist(&bgr_planes[1], 1, nullptr, cv::Mat(), g_hist, 1, &histsize, &histRanges, true, false);
    calcHist(&bgr_planes[2], 1, nullptr, cv::Mat(), r_hist, 1, &histsize, &histRanges, true, false);

    //归一化
    int hist_h = 500;//直方图的图像的高
    int hist_w = 512;//直方图的图像的宽
    int bin_w = hist_w / histsize;//直方图的等级
    dstImage = cv::Mat(hist_w, hist_h, CV_8UC3, cv::Scalar(255, 255, 255));//绘制直方图显示的图像

    normalize(b_hist, b_hist, 0, hist_h, cv::NORM_MINMAX, -1, cv::Mat());//归一化
    normalize(g_hist, g_hist, 0, hist_h, cv::NORM_MINMAX, -1, cv::Mat());
    normalize(r_hist, r_hist, 0, hist_h, cv::NORM_MINMAX, -1, cv::Mat());

    //绘制直方图
    for (int i = 1; i < histsize; ++i)
    {
        //绘制蓝色分量直方图
        line(dstImage,
             cv::Point((i - 1)*bin_w, hist_h - cvRound(b_hist.at<float>(i - 1))),
             cv::Point((i)*bin_w, hist_h - cvRound(b_hist.at<float>(i))),
             cv::Scalar(255, 0, 0),
             2,
             cv::LINE_AA);
        //绘制绿色分量直方图
        line(dstImage,
             cv::Point((i - 1)*bin_w, hist_h - cvRound(g_hist.at<float>(i - 1))),
             cv::Point((i)*bin_w, hist_h - cvRound(g_hist.at<float>(i))),
             cv::Scalar(0, 255, 0),
             2,
             cv::LINE_AA);
        //绘制红色分量直方图
        line(dstImage,
             cv::Point((i - 1)*bin_w, hist_h - cvRound(r_hist.at<float>(i - 1))),
             cv::Point((i)*bin_w, hist_h - cvRound(r_hist.at<float>(i))),
             cv::Scalar(0, 0, 255),
             2,
             cv::LINE_AA);
    }
    dstlabel_show(srcImage, dstImage);
}

//直方图（柱状）
void MainWindow::on_action_hist_colu_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(!control_widget->isHidden())
    {
        control_widget->hide();
    }

    int bins = 256;
    int hist_size[] = {bins};
    float range[] = {0, 256 };
    const float * ranges[] = {range};
    cv::Mat hist_r,hist_g,hist_b;

    int channels_r[] = {2};
    calcHist(&srcImage, 1, channels_r, cv::Mat(), hist_r, 1, hist_size, ranges, true, false );
    int channels_g[] = {1};
    calcHist(&srcImage, 1, channels_g, cv::Mat(), hist_g, 1, hist_size, ranges, true, false);
    int channels_b[] = {0};
    calcHist( &srcImage, 1, channels_b, cv::Mat(),hist_b, 1, hist_size, ranges, true, false);

    double max_val_r,max_val_g,max_val_b;
    minMaxLoc(hist_r, nullptr, &max_val_r, nullptr, nullptr);
    minMaxLoc(hist_g, nullptr, &max_val_g, nullptr, nullptr);
    minMaxLoc(hist_b, nullptr, &max_val_b, nullptr, nullptr);

    int scale = 1;
    int hist_height = 256;
    dstImage = cv::Mat::zeros(hist_height, bins*3+5, CV_8UC3);
    float bin_val_r, bin_val_g, bin_val_b;
    int intensity_r, intensity_g, intensity_b;

    for(int i = 0;i < bins;++i)
    {
        bin_val_r = hist_r.at<float>(i);
        bin_val_g = hist_g.at<float>(i);
        bin_val_b = hist_b.at<float>(i);
        intensity_r = cvRound(bin_val_r*hist_height/max_val_r);  //要绘制的高度
        intensity_g = cvRound(bin_val_g*hist_height/max_val_g);  //要绘制的高度
        intensity_b = cvRound(bin_val_b*hist_height/max_val_b);  //要绘制的高度
        rectangle(dstImage,cv::Point(i*scale,hist_height-1), cv::Point((i+1)*scale - 1, hist_height - intensity_r), CV_RGB(255,0,0));
        rectangle(dstImage,cv::Point((i+bins)*scale,hist_height-1), cv::Point((i+bins+1)*scale - 1, hist_height - intensity_g), CV_RGB(0,255,0));
        rectangle(dstImage,cv::Point((i+bins*2)*scale,hist_height-1), cv::Point((i+bins*2+1)*scale - 1, hist_height - intensity_b), CV_RGB(0,0,255));
    }
    dstlabel_show(srcImage,dstImage);
}

//基本二值化方法
void MainWindow::on_action_base_triggered()
{
    if (srcImage.empty())
        {
           return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * jiben_thresh_func = new QComboBox(center_widget_in_dock);
    jiben_thresh_func->addItem(QString("THRESH_BINARY"),cv::THRESH_BINARY);
    jiben_thresh_func->addItem(QString("THRESH_BINARY_INV"),cv::THRESH_BINARY_INV);
    jiben_thresh_func->addItem(QString("THRESH_TRUNC"),cv::THRESH_TRUNC);
    jiben_thresh_func->addItem(QString("THRESH_TOZERO"),cv::THRESH_TOZERO);
    jiben_thresh_func->addItem(QString("THRESH_TOZERO_INV"),cv::THRESH_TOZERO_INV);
    jiben_thresh_func->setView(new QListView);
    jiben_thresh_func->setFixedHeight(36);
    jiben_thresh_func->setFixedWidth(220);

    QSpinBox * jibenthresh_SpinBox = new QSpinBox(center_widget_in_dock);
    jibenthresh_SpinBox->setMinimum(1);
    jibenthresh_SpinBox->setMaximum(255);
    jibenthresh_SpinBox->setValue(1);
    jibenthresh_SpinBox->setFixedHeight(36);

    QSlider * jibenthresh_slider = new QSlider(center_widget_in_dock);
    jibenthresh_slider->setOrientation(Qt::Horizontal);
    jibenthresh_slider->setMinimum(1);
    jibenthresh_slider->setMaximum(255);
    jibenthresh_slider->setSingleStep(10);
    jibenthresh_slider->setTickInterval(10);
    jibenthresh_slider->setTickPosition(QSlider::TicksAbove);
    jibenthresh_slider->setValue(1);
    jibenthresh_slider->setFixedHeight(36);

    QSpinBox * jibenthresh_max_SpinBox = new QSpinBox(center_widget_in_dock);
    jibenthresh_max_SpinBox->setMinimum(1);
    jibenthresh_max_SpinBox->setMaximum(255);
    jibenthresh_max_SpinBox->setValue(255);
    jibenthresh_max_SpinBox->setFixedHeight(36);

    QSlider * jibenthresh_max_slider = new QSlider(center_widget_in_dock);
    jibenthresh_max_slider->setOrientation(Qt::Horizontal);
    jibenthresh_max_slider->setMinimum(1);
    jibenthresh_max_slider->setMaximum(255);
    jibenthresh_max_slider->setSingleStep(10);
    jibenthresh_max_slider->setTickInterval(10);
    jibenthresh_max_slider->setTickPosition(QSlider::TicksAbove);
    jibenthresh_max_slider->setValue(255);
    jibenthresh_max_slider->setFixedHeight(36);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->setHorizontalSpacing(30);
    grildlayout->addWidget(jiben_thresh_func,0,0);
    grildLayoutAddWidgetLable("设置当前的阈值：",0,1);
    grildlayout->addWidget(jibenthresh_SpinBox,0,2);
    grildlayout->addWidget(jibenthresh_slider,0,3);
    QLabel* lable = new QLabel("THRESH_BINARY | THRESH_BINARY_INV的最大值：");
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::black);
    lable->setPalette(palette);
    grildlayout->addWidget(lable, 1,0,1,2);
    grildlayout->addWidget(jibenthresh_max_SpinBox,1,2);
    grildlayout->addWidget(jibenthresh_max_slider,1,3);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    connect(jibenthresh_slider, &QAbstractSlider::valueChanged, jibenthresh_SpinBox, &QSpinBox::setValue);
    connect(jibenthresh_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), jibenthresh_slider, &QAbstractSlider::setValue);
    connect(jibenthresh_max_slider, &QAbstractSlider::valueChanged, jibenthresh_max_SpinBox, &QSpinBox::setValue);
    connect(jibenthresh_max_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), jibenthresh_max_slider, &QAbstractSlider::setValue);

    auto fun = [=]
    {
        int select_threshway = jiben_thresh_func->currentData(Qt::UserRole).toInt();//0~4
        int maxvalue;

        if(select_threshway == 0 || select_threshway == 1)
        {
            maxvalue = jibenthresh_max_SpinBox->text().toInt();
            jibenthresh_max_slider->setEnabled(true);
            jibenthresh_max_SpinBox->setEnabled(true);
        }
        else
        {
            maxvalue = 255;
            jibenthresh_max_slider->setEnabled(false);
            jibenthresh_max_SpinBox->setEnabled(false);
        }

        int thresh_value = jibenthresh_SpinBox->text().toInt();

        cv::Mat temp_gray;
        cvtColor(srcImage,temp_gray,cv::COLOR_RGB2GRAY);
        threshold(temp_gray,dstImage,thresh_value,maxvalue,select_threshway);
        dstlabel_indexed8_show(dstImage);
    };
    jiben_thresh_func->setStyleSheet("background-color:black;color:white");
    jibenthresh_slider->setStyleSheet("background-color:black;color:white");
    jibenthresh_SpinBox->setStyleSheet("background-color:black;color:white");
    jibenthresh_max_slider->setStyleSheet("background-color:black;color:white");
    jibenthresh_max_SpinBox->setStyleSheet("background-color:black;color:white");
    connect(jibenthresh_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(jibenthresh_slider, &QAbstractSlider::valueChanged, fun);
    connect(jibenthresh_max_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(jibenthresh_max_slider, &QAbstractSlider::valueChanged, fun);
    connect(jiben_thresh_func, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    fun();
}

//THRESH_OTSU + 基本二值化方法
void MainWindow::on_actionTHRESH_OTSU_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * jiben_THRESH_OTSU_func = new QComboBox(center_widget_in_dock);
    jiben_THRESH_OTSU_func->addItem(QString("THRESH_OTSU + THRESH_BINARY"),0);
    jiben_THRESH_OTSU_func->addItem(QString("THRESH_OTSU + THRESH_BINARY_INV"),1);
    jiben_THRESH_OTSU_func->addItem(QString("THRESH_OTSU + THRESH_TRUNC"),2);
    jiben_THRESH_OTSU_func->addItem(QString("THRESH_OTSU + THRESH_TOZERO"),3);
    jiben_THRESH_OTSU_func->addItem(QString("THRESH_OTSU + THRESH_TOZERO_INV"),4);
    jiben_THRESH_OTSU_func->setView(new QListView);
    jiben_THRESH_OTSU_func->setStyleSheet("background:black;color:white");

    QSpinBox * THRESH_OTSU_max_SpinBox = new QSpinBox(center_widget_in_dock);
    THRESH_OTSU_max_SpinBox->setMinimum(1);
    THRESH_OTSU_max_SpinBox->setMaximum(255);
    THRESH_OTSU_max_SpinBox->setValue(255);//

    QSlider * THRESH_OTSU_max_slider = new QSlider(center_widget_in_dock);
    THRESH_OTSU_max_slider->setOrientation(Qt::Horizontal);
    THRESH_OTSU_max_slider->setMinimum(1);
    THRESH_OTSU_max_slider->setMaximum(255);
    THRESH_OTSU_max_slider->setSingleStep(10);
    THRESH_OTSU_max_slider->setTickInterval(10); //设置刻度间隔
    THRESH_OTSU_max_slider->setTickPosition(QSlider::TicksAbove);
    THRESH_OTSU_max_slider->setValue(1);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(jiben_THRESH_OTSU_func,0,0);
    grildLayoutAddWidgetLable("THRESH_BINARY | THRESH_BINARY_INV的最大值：",1,0);
    grildlayout->addWidget(THRESH_OTSU_max_SpinBox,1,1);
    grildlayout->addWidget(THRESH_OTSU_max_slider,1,2);
    grildlayout->setContentsMargins(20,20,20,20);

    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    connect(THRESH_OTSU_max_slider, &QAbstractSlider::valueChanged, THRESH_OTSU_max_SpinBox, &QSpinBox::setValue);
    connect(THRESH_OTSU_max_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), THRESH_OTSU_max_slider, &QAbstractSlider::setValue);

    auto fun = [=]
    {
        int jiben_THRESH_OTSU_funcvarint = jiben_THRESH_OTSU_func->currentData(Qt::UserRole).toInt();//0~4
        int maxvalue;

        if(jiben_THRESH_OTSU_funcvarint == 0 || jiben_THRESH_OTSU_funcvarint == 1)
        {
            maxvalue = THRESH_OTSU_max_SpinBox->text().toInt();
            THRESH_OTSU_max_SpinBox->setEnabled(true);
            THRESH_OTSU_max_slider->setEnabled(true);
        }
        else
        {
            maxvalue = 255;
            THRESH_OTSU_max_SpinBox->setEnabled(false);
            THRESH_OTSU_max_slider->setEnabled(false);
        }

        cv::Mat temp_gray;
        cvtColor(srcImage,temp_gray,cv::COLOR_RGB2GRAY);

        int thresh_type = 0;
        switch (jiben_THRESH_OTSU_funcvarint)
        {
            case 0: thresh_type = cv::THRESH_OTSU | cv::THRESH_BINARY; break;
            case 1: thresh_type = cv::THRESH_OTSU | cv::THRESH_BINARY_INV; break;
            case 2: thresh_type = cv::THRESH_OTSU | cv::THRESH_TRUNC;  break;
            case 3: thresh_type = cv::THRESH_OTSU | cv::THRESH_TOZERO;  break;
            case 4: thresh_type = cv::THRESH_OTSU | cv::THRESH_TOZERO_INV;  break;
        }
        //当使用OTSU的时候，第三个参数是不起作用的
        threshold(temp_gray,dstImage,0,maxvalue,thresh_type);
        dstlabel_indexed8_show(dstImage);
    };
    THRESH_OTSU_max_SpinBox->setStyleSheet("background-color:black;color:white");
    THRESH_OTSU_max_slider->setStyleSheet("background-color:black;color:white");
    connect(THRESH_OTSU_max_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged),fun);
    connect(THRESH_OTSU_max_slider, &QAbstractSlider::valueChanged,fun);
    connect(jiben_THRESH_OTSU_func, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    fun();
}

//THRESH_TRIANGLE + 基本二值化方法
void MainWindow::on_actionTHRESH_TRIANGLE_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }
    QComboBox * jiben_THRESH_TRIANGLE_func = new QComboBox(center_widget_in_dock);
    jiben_THRESH_TRIANGLE_func->addItem(QString("THRESH_TRIANGLE + THRESH_BINARY"),0);
    jiben_THRESH_TRIANGLE_func->addItem(QString("THRESH_TRIANGLE + THRESH_BINARY_INV"),1);
    jiben_THRESH_TRIANGLE_func->addItem(QString("THRESH_TRIANGLE + THRESH_TRUNC"),2);
    jiben_THRESH_TRIANGLE_func->addItem(QString("THRESH_TRIANGLE + THRESH_TOZERO"),3);
    jiben_THRESH_TRIANGLE_func->addItem(QString("THRESH_TRIANGLE + THRESH_TOZERO_INV"),4);
    jiben_THRESH_TRIANGLE_func->setView(new QListView);

    QSpinBox * THRESH_TRIANGLE_max_SpinBox = new QSpinBox(center_widget_in_dock);
    THRESH_TRIANGLE_max_SpinBox->setMinimum(1);
    THRESH_TRIANGLE_max_SpinBox->setMaximum(255);
    THRESH_TRIANGLE_max_SpinBox->setValue(255);//

    QSlider * THRESH_TRIANGLE_max_slider = new QSlider(center_widget_in_dock);
    THRESH_TRIANGLE_max_slider->setOrientation(Qt::Horizontal);
    THRESH_TRIANGLE_max_slider->setMinimum(1);
    THRESH_TRIANGLE_max_slider->setMaximum(255);
    THRESH_TRIANGLE_max_slider->setSingleStep(10);
    THRESH_TRIANGLE_max_slider->setTickInterval(10);
    THRESH_TRIANGLE_max_slider->setTickPosition(QSlider::TicksAbove);
    THRESH_TRIANGLE_max_slider->setValue(255);//

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(jiben_THRESH_TRIANGLE_func,0,0);
    grildLayoutAddWidgetLable("THRESH_BINARY | THRESH_BINARY_INV的最大值：",1,0);
    grildlayout->addWidget(THRESH_TRIANGLE_max_SpinBox,1,1);
    grildlayout->addWidget(THRESH_TRIANGLE_max_slider,1,2);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    connect(THRESH_TRIANGLE_max_slider, &QAbstractSlider::valueChanged, THRESH_TRIANGLE_max_SpinBox, &QSpinBox::setValue);
    connect(THRESH_TRIANGLE_max_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), THRESH_TRIANGLE_max_slider, &QAbstractSlider::setValue);

    auto fun = [=]
    {
        int jiben_THRESH_TRIANGLE_funcvarint = jiben_THRESH_TRIANGLE_func->currentData(Qt::UserRole).toInt();//0~4

        int maxvalue;
        if(jiben_THRESH_TRIANGLE_funcvarint == 0 || jiben_THRESH_TRIANGLE_funcvarint == 1)
        {
            maxvalue = THRESH_TRIANGLE_max_SpinBox->text().toInt();
            THRESH_TRIANGLE_max_slider->setEnabled(true);
            THRESH_TRIANGLE_max_SpinBox->setEnabled(true);
        }
        else
        {
            maxvalue = 255;
            THRESH_TRIANGLE_max_slider->setEnabled(false);
            THRESH_TRIANGLE_max_SpinBox->setEnabled(false);
        }

        int thresh_type = 0;
        cv::Mat temp_gray;

        switch (jiben_THRESH_TRIANGLE_funcvarint)
        {
            case 0: thresh_type = cv::THRESH_TRIANGLE | cv::THRESH_BINARY; break;
            case 1: thresh_type = cv::THRESH_TRIANGLE | cv::THRESH_BINARY_INV; break;
            case 2: thresh_type = cv::THRESH_TRIANGLE | cv::THRESH_TRUNC;  break;
            case 3: thresh_type = cv::THRESH_TRIANGLE | cv::THRESH_TOZERO;  break;
            case 4: thresh_type = cv::THRESH_TRIANGLE | cv::THRESH_TOZERO_INV;  break;
        }

        cvtColor(srcImage,temp_gray,cv::COLOR_RGB2GRAY);

        //当使用THRESH_TRIANGLE的时候，第三个参数是不起作用的
        threshold(temp_gray,dstImage,250,maxvalue,thresh_type);
        dstlabel_indexed8_show(dstImage);
    };
    THRESH_TRIANGLE_max_slider->setStyleSheet("background-color:black;color:white");
    THRESH_TRIANGLE_max_SpinBox->setStyleSheet("background-color:black;color:white");
    jiben_THRESH_TRIANGLE_func->setStyleSheet("background:black;color:white");
    connect(THRESH_TRIANGLE_max_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(THRESH_TRIANGLE_max_slider, &QAbstractSlider::valueChanged, fun);
    connect(jiben_THRESH_TRIANGLE_func, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    fun();
}

//自适应二值化
void MainWindow::on_action_adapt_triggered()
{
    if (srcImage.empty())
        {
            //show_info("请先打开一张图片",Qt::AlignCenter);
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }
    QComboBox * jiben_THRESH_adapt_meth = new QComboBox(center_widget_in_dock);
    jiben_THRESH_adapt_meth->addItem(QString("ADAPTIVE_THRESH_MEAN_C"),cv::ADAPTIVE_THRESH_MEAN_C);
    jiben_THRESH_adapt_meth->addItem(QString("ADAPTIVE_THRESH_GAUSSIAN_C"),cv::ADAPTIVE_THRESH_GAUSSIAN_C);
    jiben_THRESH_adapt_meth->setView(new QListView);
    jiben_THRESH_adapt_meth->setFixedWidth(300);

    QComboBox * jiben_THRESH_adapt_type = new QComboBox(center_widget_in_dock);
    jiben_THRESH_adapt_type->addItem(QString("THRESH_BINARY"),cv::THRESH_BINARY);
    jiben_THRESH_adapt_type->addItem(QString("THRESH_BINARY_INV"),cv::THRESH_BINARY_INV);
    jiben_THRESH_adapt_type->setView(new QListView);

    QSpinBox * adapt_max_SpinBox = new QSpinBox(center_widget_in_dock);
    adapt_max_SpinBox->setMinimum(1);
    adapt_max_SpinBox->setMaximum(255);
    adapt_max_SpinBox->setValue(255);//

    QSlider * adapt_max_slider = new QSlider(center_widget_in_dock);
    adapt_max_slider->setOrientation(Qt::Horizontal);
    adapt_max_slider->setMinimum(1);
    adapt_max_slider->setMaximum(255);
    adapt_max_slider->setSingleStep(10);
    adapt_max_slider->setTickInterval(10);
    adapt_max_slider->setTickPosition(QSlider::TicksAbove);
    adapt_max_slider->setValue(255);//

    QComboBox * adapt_blocksize = new QComboBox(center_widget_in_dock);
    for(int i = 3;i <= 20;i += 2)
    {
        adapt_blocksize->addItem(QString("邻域范围：%1").arg(i),i);
    }
    adapt_blocksize->setView(new QListView);

    QSpinBox * adapt_C_SpinBox = new QSpinBox(center_widget_in_dock);
    adapt_C_SpinBox->setMinimum(-50);
    adapt_C_SpinBox->setMaximum(50);
    adapt_C_SpinBox->setSingleStep(1);
    adapt_C_SpinBox->setValue(1);//

    QSlider *adapt_C_slider = new QSlider(center_widget_in_dock);
    adapt_C_slider->setOrientation(Qt::Horizontal);
    adapt_C_slider->setMinimum(-50);
    adapt_C_slider->setMaximum(50);
    adapt_C_slider->setSingleStep(1);
    adapt_C_slider->setTickInterval(4);
    adapt_C_slider->setTickPosition(QSlider::TicksAbove);
    adapt_C_slider->setValue(1);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(adapt_blocksize,0,0);
    grildLayoutAddWidgetLable("阈值类型：",0,1);
    grildlayout->addWidget(jiben_THRESH_adapt_type,0,2);
    grildLayoutAddWidgetLable("自适应阈值算法：",0,3);
    grildlayout->addWidget(jiben_THRESH_adapt_meth,0,4);
    grildLayoutAddWidgetLable("最大阈值：",1,0);
    grildlayout->addWidget(adapt_max_SpinBox,1,1);
    grildlayout->addWidget(adapt_max_slider,1,2,1,20);
    grildLayoutAddWidgetLable("偏移常量：",2,0);
    grildlayout->addWidget(adapt_C_SpinBox,2,1);
    grildlayout->addWidget(adapt_C_slider,2,2,1,20);
    grildlayout->setContentsMargins(20,20,20,20);
    center_widget_in_dock->setLayout(grildlayout);

    connect(adapt_max_slider, &QAbstractSlider::valueChanged, adapt_max_SpinBox, &QSpinBox::setValue);
    connect(adapt_max_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), adapt_max_slider, &QAbstractSlider::setValue);
    connect(adapt_C_slider, &QAbstractSlider::valueChanged, adapt_C_SpinBox, &QSpinBox::setValue);
    connect(adapt_C_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), adapt_C_slider, &QAbstractSlider::setValue);

    auto fun = [=]
    {
        cv::Mat temp_gray;

        cvtColor(srcImage,temp_gray,cv::COLOR_RGB2GRAY);
        adaptiveThreshold(temp_gray,
                          dstImage,
                          adapt_max_SpinBox->text().toInt(),
                          jiben_THRESH_adapt_meth->currentData(Qt::UserRole).toInt(),
                          jiben_THRESH_adapt_type->currentData(Qt::UserRole).toInt(),
                          adapt_blocksize->currentData(Qt::UserRole).toInt(),
                          adapt_C_SpinBox->text().toInt());
        dstlabel_indexed8_show(dstImage);
    };
    adapt_C_slider->setStyleSheet("background-color:black;color:white");
    adapt_C_SpinBox->setStyleSheet("background-color:black;color:white");
    adapt_blocksize->setStyleSheet("background-color:black;color:white");
    adapt_max_slider->setStyleSheet("background-color:black;color:white");
    adapt_max_SpinBox->setStyleSheet("background-color:black;color:white");
    jiben_THRESH_adapt_meth->setStyleSheet("background:black;color:white");
    jiben_THRESH_adapt_type->setStyleSheet("background:black;color:white");
    connect(adapt_blocksize, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    connect(jiben_THRESH_adapt_type, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    connect(jiben_THRESH_adapt_meth, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    connect(adapt_max_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(adapt_max_slider, &QAbstractSlider::valueChanged, fun);
    connect(adapt_C_slider, &QAbstractSlider::valueChanged, fun);
    connect(adapt_C_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    fun();
}

//调节亮度/对比度
void MainWindow::on_action_tiaojie_liangdu_duibidu_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    //调节亮度
    QSpinBox * liangdu_SpinBox = new QSpinBox(center_widget_in_dock);
    liangdu_SpinBox->setMinimum(-200);
    liangdu_SpinBox->setMaximum(200);
    liangdu_SpinBox->setValue(0);//
    liangdu_SpinBox->setFixedHeight(36);

    QSlider * liangdu_slider = new QSlider(center_widget_in_dock);
    liangdu_slider->setOrientation(Qt::Horizontal);
    liangdu_slider->setMinimum(-200);
    liangdu_slider->setMaximum(200);
    liangdu_slider->setSingleStep(10);
    liangdu_slider->setTickInterval(10);
    liangdu_slider->setTickPosition(QSlider::TicksAbove);
    liangdu_slider->setValue(0);
    liangdu_slider->setFixedHeight(36);

    //调节对比度
    QLineEdit * duibidu_LineEdit = new QLineEdit(center_widget_in_dock);
    duibidu_LineEdit->setFocusPolicy(Qt::NoFocus);//无法获得焦点，即无法编辑
    duibidu_LineEdit->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    duibidu_LineEdit->setFixedHeight(36);
    duibidu_LineEdit->setText("1.00");
    duibidu_LineEdit->setStyleSheet("background-color:black;color:white");

    QSlider * duibidu_slider = new QSlider(center_widget_in_dock);
    duibidu_slider->setOrientation(Qt::Horizontal);
    duibidu_slider->setMinimum(1);
    duibidu_slider->setMaximum(1000);
    duibidu_slider->setSingleStep(5);
    duibidu_slider->setTickInterval(40);
    duibidu_slider->setTickPosition(QSlider::TicksAbove);
    duibidu_slider->setValue(100);//
    duibidu_slider->setFixedHeight(36);
    duibidu_slider->setStyleSheet("background-color:black;color:white");

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("设置亮度：",0,0);
    grildlayout->addWidget(liangdu_SpinBox,0,1);
    grildlayout->addWidget(liangdu_slider,0,2);
    grildLayoutAddWidgetLable("设置对比度：",1,0);
    grildlayout->addWidget(duibidu_LineEdit,1,1);
    grildlayout->addWidget(duibidu_slider,1,2);
    grildlayout->setContentsMargins(20,20,20,20);

    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        double temp = double(duibidu_slider->value())/100;
        duibidu_LineEdit->setText(QString::number(temp,10,2));

        srcImage.convertTo(dstImage,-1,temp,liangdu_slider->value());
        dstlabel_show(srcImage, dstImage);
    };
    liangdu_slider->setStyleSheet("background-color:black;color:white");
    liangdu_SpinBox->setStyleSheet("background-color:black;color:white");
    connect(liangdu_slider, &QAbstractSlider::valueChanged, liangdu_SpinBox, &QSpinBox::setValue);
    connect(liangdu_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), liangdu_slider, &QAbstractSlider::setValue);
    connect(duibidu_slider, &QAbstractSlider::valueChanged,fun);
    connect(liangdu_slider, &QAbstractSlider::valueChanged,fun);
    connect(liangdu_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged),fun);
    fun();
}

//仿射变换
void MainWindow::on_action_warpAffine_triggered()
{
    if (srcImage.empty())
        {
           return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QSpinBox * warpAffine_center_x_SpinBox = new QSpinBox(center_widget_in_dock);
    warpAffine_center_x_SpinBox->setMinimum(0);
    warpAffine_center_x_SpinBox->setMaximum(99);
    warpAffine_center_x_SpinBox->setValue(50);//
    warpAffine_center_x_SpinBox->setPrefix("0.");//前缀
    warpAffine_center_x_SpinBox->setFixedHeight(36);

    QSlider * warpAffine_center_x_slider = new QSlider(center_widget_in_dock);
    warpAffine_center_x_slider->setOrientation(Qt::Horizontal);
    warpAffine_center_x_slider->setMinimum(0);
    warpAffine_center_x_slider->setMaximum(99);
    warpAffine_center_x_slider->setSingleStep(10);
    warpAffine_center_x_slider->setTickInterval(5);
    warpAffine_center_x_slider->setTickPosition(QSlider::TicksAbove);
    warpAffine_center_x_slider->setValue(50);//
    warpAffine_center_x_slider->setFixedHeight(36);

    QSpinBox * warpAffine_center_y_SpinBox = new QSpinBox(center_widget_in_dock);
    warpAffine_center_y_SpinBox->setMinimum(0);
    warpAffine_center_y_SpinBox->setMaximum(99);
    warpAffine_center_y_SpinBox->setValue(50);//
    warpAffine_center_y_SpinBox->setPrefix("0.");//前缀
    warpAffine_center_y_SpinBox->setFixedHeight(36);

    QSlider * warpAffine_center_y_slider = new QSlider(center_widget_in_dock);
    warpAffine_center_y_slider->setOrientation(Qt::Horizontal);
    warpAffine_center_y_slider->setMinimum(0);
    warpAffine_center_y_slider->setMaximum(99);
    warpAffine_center_y_slider->setSingleStep(10);
    warpAffine_center_y_slider->setTickInterval(5);
    warpAffine_center_y_slider->setTickPosition(QSlider::TicksAbove);
    warpAffine_center_y_slider->setValue(50);//
    warpAffine_center_y_slider->setFixedHeight(36);

    QSpinBox * warpAffine_angle_SpinBox = new QSpinBox(center_widget_in_dock);
    warpAffine_angle_SpinBox->setMinimum(-180);
    warpAffine_angle_SpinBox->setMaximum(180);
    warpAffine_angle_SpinBox->setValue(0);//
    warpAffine_angle_SpinBox->setFixedHeight(36);

    QSlider * warpAffine_angle_slider = new QSlider(center_widget_in_dock);
    warpAffine_angle_slider->setOrientation(Qt::Horizontal);
    warpAffine_angle_slider->setMinimum(-180);
    warpAffine_angle_slider->setMaximum(180);
    warpAffine_angle_slider->setSingleStep(10);
    warpAffine_angle_slider->setTickInterval(20);
    warpAffine_angle_slider->setTickPosition(QSlider::TicksAbove);
    warpAffine_angle_slider->setValue(0);//
    warpAffine_angle_slider->setFixedHeight(36);

    QLineEdit * warpAffine_scale_LineEdit = new QLineEdit(center_widget_in_dock);
    warpAffine_scale_LineEdit->setFocusPolicy(Qt::NoFocus);//无法获得焦点，即无法编辑
    warpAffine_scale_LineEdit->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    warpAffine_scale_LineEdit->setText("1.00");
    warpAffine_scale_LineEdit->setFixedHeight(36);

    QSlider * warpAffine_scale_Slider = new QSlider(center_widget_in_dock);
    warpAffine_scale_Slider->setOrientation(Qt::Horizontal);
    warpAffine_scale_Slider->setMinimum(1);
    warpAffine_scale_Slider->setMaximum(1000);
    warpAffine_scale_Slider->setSingleStep(50);
    warpAffine_scale_Slider->setTickInterval(50);
    warpAffine_scale_Slider->setTickPosition(QSlider::TicksAbove);
    warpAffine_scale_Slider->setValue(100);
    warpAffine_scale_Slider->setFixedHeight(36);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("旋转中心--X轴：",0,0);
    grildlayout->addWidget(warpAffine_center_x_SpinBox,0,1);
    grildlayout->addWidget(warpAffine_center_x_slider,0,2);
    grildLayoutAddWidgetLable("旋转中心--Y轴：",1,0);
    grildlayout->addWidget(warpAffine_center_y_SpinBox,1,1);
    grildlayout->addWidget(warpAffine_center_y_slider,1,2);
    grildLayoutAddWidgetLable("旋转角度：",2,0);
    grildlayout->addWidget(warpAffine_angle_SpinBox,2,1);
    grildlayout->addWidget(warpAffine_angle_slider,2,2);
    grildLayoutAddWidgetLable("缩放比例：",3,0);
    grildlayout->addWidget(warpAffine_scale_LineEdit,3,1);
    grildlayout->addWidget(warpAffine_scale_Slider,3,2);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    warpAffine_angle_slider->setStyleSheet("background-color:black;color:white");
    warpAffine_scale_Slider->setStyleSheet("background-color:black;color:white");
    warpAffine_angle_SpinBox->setStyleSheet("background-color:black;color:white");
    warpAffine_scale_LineEdit->setStyleSheet("background-color:black;color:white");
    warpAffine_center_x_slider->setStyleSheet("background-color:black;color:white");
    warpAffine_center_y_slider->setStyleSheet("background-color:black;color:white");
    warpAffine_center_x_SpinBox->setStyleSheet("background-color:black;color:white");
    warpAffine_center_y_SpinBox->setStyleSheet("background-color:black;color:white");
    connect(warpAffine_center_x_slider, &QAbstractSlider::valueChanged, warpAffine_center_x_SpinBox, &QSpinBox::setValue);
    connect(warpAffine_center_x_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), warpAffine_center_x_slider, &QAbstractSlider::setValue);
    connect(warpAffine_center_y_slider, &QAbstractSlider::valueChanged, warpAffine_center_y_SpinBox, &QSpinBox::setValue);
    connect(warpAffine_center_y_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), warpAffine_center_y_slider, &QAbstractSlider::setValue);
    connect(warpAffine_angle_slider, &QAbstractSlider::valueChanged, warpAffine_angle_SpinBox, &QSpinBox::setValue);
    connect(warpAffine_angle_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), warpAffine_angle_slider, &QAbstractSlider::setValue);

    auto fun = [=]
    {
        if(warpAffine_center_x_slider->value() >=10)
        {
            warpAffine_center_x_SpinBox->setPrefix("0.");//前缀
        }
        else
        {
            warpAffine_center_x_SpinBox->setPrefix("0.0");//前缀
        }

        if(warpAffine_center_y_slider->value() >=10)
        {
            warpAffine_center_y_SpinBox->setPrefix("0.");//前缀
        }
        else
        {
            warpAffine_center_y_SpinBox->setPrefix("0.0");//前缀
        }

        double temp = double(warpAffine_scale_Slider->value())/100;
        warpAffine_scale_LineEdit->setText(QString::number(temp,10,2));

        cv::Point2f center(int(srcImage.cols * warpAffine_center_x_SpinBox->text().toCaseFolded().toDouble()),
                           int(srcImage.rows * warpAffine_center_y_SpinBox->text().toCaseFolded().toDouble()));

        cv::Mat M = getRotationMatrix2D(center,
                                        warpAffine_angle_SpinBox->value(),
                                        temp);//计算旋转加缩放的变换矩阵
        warpAffine(srcImage, dstImage, M, srcImage.size(),1,0,cv::Scalar(rand()&255,rand()&255,rand()&255));
        dstlabel_show(srcImage,dstImage);
    };

    connect(warpAffine_center_x_slider, &QAbstractSlider::valueChanged, fun);
    connect(warpAffine_center_x_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(warpAffine_center_y_slider, &QAbstractSlider::valueChanged, fun);
    connect(warpAffine_center_y_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(warpAffine_angle_slider, &QAbstractSlider::valueChanged, fun);
    connect(warpAffine_angle_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(warpAffine_scale_Slider, &QAbstractSlider::valueChanged, fun);
    fun();
}

//笛卡尔坐标与极坐标相互转换
void MainWindow::on_action_logPolar_triggered()
{
    if (srcImage.empty())
        {
           return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * logPolar_type = new QComboBox(center_widget_in_dock);
    logPolar_type->setFixedHeight(36);
    logPolar_type->setView(new QListView());
    logPolar_type->addItem(QString("笛卡尔坐标 --> 极坐标"),1);
    logPolar_type->addItem(QString("极坐标 --> 笛卡尔坐标"),2);
    connect(logPolar_type, SIGNAL(activated(const QString &)),this, SLOT(process_logPolar()));

    QSpinBox * logPolar_center_x_SpinBox = new QSpinBox(center_widget_in_dock);
    logPolar_center_x_SpinBox->setMinimum(1);
    logPolar_center_x_SpinBox->setMaximum(99);
    logPolar_center_x_SpinBox->setValue(50);//
    logPolar_center_x_SpinBox->setPrefix("0.");//前缀
    logPolar_center_x_SpinBox->setFixedHeight(36);

    QSlider * logPolar_center_x_slider = new QSlider(center_widget_in_dock);
    logPolar_center_x_slider->setOrientation(Qt::Horizontal);
    logPolar_center_x_slider->setMinimum(1);
    logPolar_center_x_slider->setMaximum(99);
    logPolar_center_x_slider->setSingleStep(10);
    logPolar_center_x_slider->setTickInterval(5);
    logPolar_center_x_slider->setTickPosition(QSlider::TicksAbove);
    logPolar_center_x_slider->setValue(50);
    logPolar_center_x_slider->setFixedHeight(36);

    QSpinBox * logPolar_center_y_SpinBox = new QSpinBox(center_widget_in_dock);
    logPolar_center_y_SpinBox->setMinimum(1);
    logPolar_center_y_SpinBox->setMaximum(99);
    logPolar_center_y_SpinBox->setValue(50);
    logPolar_center_y_SpinBox->setPrefix("0.");//前缀
    logPolar_center_y_SpinBox->setFixedHeight(36);

    QSlider * logPolar_center_y_slider = new QSlider(center_widget_in_dock);
    logPolar_center_y_slider->setOrientation(Qt::Horizontal);
    logPolar_center_y_slider->setMinimum(1);
    logPolar_center_y_slider->setMaximum(99);
    logPolar_center_y_slider->setSingleStep(10);
    logPolar_center_y_slider->setTickInterval(5);
    logPolar_center_y_slider->setTickPosition(QSlider::TicksAbove);
    logPolar_center_y_slider->setValue(50);
    logPolar_center_y_slider->setFixedHeight(36);

    QSpinBox * logPolar_Magnitude_SpinBox = new QSpinBox(center_widget_in_dock);
    logPolar_Magnitude_SpinBox->setMinimum(1);
    logPolar_Magnitude_SpinBox->setMaximum(800);
    logPolar_Magnitude_SpinBox->setValue(80);
    logPolar_Magnitude_SpinBox->setFixedHeight(36);

    QSlider * logPolar_Magnitude_slider = new QSlider(center_widget_in_dock);
    logPolar_Magnitude_slider->setOrientation(Qt::Horizontal);
    logPolar_Magnitude_slider->setMinimum(1);
    logPolar_Magnitude_slider->setMaximum(800);
    logPolar_Magnitude_slider->setSingleStep(10);
    logPolar_Magnitude_slider->setTickInterval(20);
    logPolar_Magnitude_slider->setTickPosition(QSlider::TicksAbove);
    logPolar_Magnitude_slider->setValue(80);
    logPolar_Magnitude_slider->setFixedHeight(36);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(logPolar_type,0,0,1,2);
    grildLayoutAddWidgetLable("变换中心--X轴：",1,0);
    grildlayout->addWidget(logPolar_center_x_SpinBox,1,1);
    grildlayout->addWidget(logPolar_center_x_slider,1,2);
    grildLayoutAddWidgetLable("变换中心--Y轴：",2,0);
    grildlayout->addWidget(logPolar_center_y_SpinBox,2,1);
    grildlayout->addWidget(logPolar_center_y_slider,2,2);
    grildLayoutAddWidgetLable("变换幅度尺度参数：",3,0);
    grildlayout->addWidget(logPolar_Magnitude_SpinBox,3,1);
    grildlayout->addWidget(logPolar_Magnitude_slider,3,2);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    logPolar_center_x_slider->setStyleSheet("background-color:black;color:white");
    logPolar_center_y_slider->setStyleSheet("background-color:black;color:white");
    logPolar_Magnitude_slider->setStyleSheet("background-color:black;color:white");
    logPolar_center_x_SpinBox->setStyleSheet("background-color:black;color:white");
    logPolar_center_y_SpinBox->setStyleSheet("background-color:black;color:white");
    logPolar_Magnitude_SpinBox->setStyleSheet("background-color:black;color:white");
    connect(logPolar_center_x_slider, &QAbstractSlider::valueChanged, logPolar_center_x_SpinBox, &QSpinBox::setValue);
    connect(logPolar_center_x_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), logPolar_center_x_slider, &QAbstractSlider::setValue);
    connect(logPolar_center_y_slider, &QAbstractSlider::valueChanged, logPolar_center_y_SpinBox, &QSpinBox::setValue);
    connect(logPolar_center_y_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), logPolar_center_y_slider, &QAbstractSlider::setValue);
    connect(logPolar_Magnitude_slider, &QAbstractSlider::valueChanged, logPolar_Magnitude_SpinBox, &QSpinBox::setValue);
    connect(logPolar_Magnitude_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), logPolar_Magnitude_slider, &QAbstractSlider::setValue);

    auto fun = [=]
    {
        if(logPolar_center_x_slider->value() >=10)
        {
            logPolar_center_x_SpinBox->setPrefix("0.");//前缀
        }
        else
        {
            logPolar_center_x_SpinBox->setPrefix("0.0");//前缀
        }

        if(logPolar_center_y_slider->value() >=10)
        {
            logPolar_center_y_SpinBox->setPrefix("0.");//前缀
        }
        else
        {
            logPolar_center_y_SpinBox->setPrefix("0.0");//前缀
        }

        cv::Point2f center(int(srcImage.cols * logPolar_center_x_SpinBox->text().toCaseFolded().toDouble()),
                       int(srcImage.rows * logPolar_center_y_SpinBox->text().toCaseFolded().toDouble()));

        int flags;
        if(logPolar_type->currentData(Qt::UserRole).toInt() == 1)
        {
            flags = cv::WARP_FILL_OUTLIERS;
        }
        else
        {
            flags = cv::WARP_INVERSE_MAP;
        }

        logPolar(srcImage,dstImage,center,logPolar_Magnitude_SpinBox->value(),flags);
        dstlabel_show(srcImage,dstImage);
    };

    connect(logPolar_center_x_slider, &QAbstractSlider::valueChanged, fun);
    connect(logPolar_center_x_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(logPolar_center_y_slider, &QAbstractSlider::valueChanged, fun);
    connect(logPolar_center_y_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(logPolar_Magnitude_slider, &QAbstractSlider::valueChanged, fun);
    connect(logPolar_Magnitude_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    fun();
}

//线性变换：y = |kx + b|
void MainWindow::on_action_kx_b_triggered()
{
    if (srcImage.empty())
        {
           return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QSlider * kx_b_k_slider = new QSlider(center_widget_in_dock);
    kx_b_k_slider->setOrientation(Qt::Horizontal);
    kx_b_k_slider->setMinimum(-100);
    kx_b_k_slider->setMaximum(100);
    kx_b_k_slider->setSingleStep(10);
    kx_b_k_slider->setTickInterval(10);
    kx_b_k_slider->setTickPosition(QSlider::TicksAbove);
    kx_b_k_slider->setValue(10);//

    QLineEdit * kx_b_k_LineEdit = new QLineEdit(center_widget_in_dock);
    kx_b_k_LineEdit->setFocusPolicy(Qt::NoFocus);//无法获得焦点，即无法编辑
    kx_b_k_LineEdit->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    kx_b_k_LineEdit->setText("1.0");

    QSlider * kx_b_b_slider = new QSlider(center_widget_in_dock);
    kx_b_b_slider->setOrientation(Qt::Horizontal);
    kx_b_b_slider->setMinimum(-2000);
    kx_b_b_slider->setMaximum(2000);
    kx_b_b_slider->setSingleStep(100);
    kx_b_b_slider->setTickInterval(100);
    kx_b_b_slider->setTickPosition(QSlider::TicksAbove);
    kx_b_b_slider->setValue(100);//

    QLineEdit * kx_b_b_LineEdit = new QLineEdit(center_widget_in_dock);
    kx_b_b_LineEdit->setFocusPolicy(Qt::NoFocus);//无法获得焦点，即无法编辑
    kx_b_b_LineEdit->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);
    kx_b_b_LineEdit->setText("10.0");

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("缩放因数k：",0,0);
    grildlayout->addWidget(kx_b_k_LineEdit,0,1);
    grildlayout->addWidget(kx_b_k_slider,0,2);
    grildLayoutAddWidgetLable("偏移量b：",1,0);
    grildlayout->addWidget(kx_b_b_LineEdit,1,1);
    grildlayout->addWidget(kx_b_b_slider,1,2);
    grildlayout->setContentsMargins(20,20,20,20);

    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        double tempk = double(kx_b_k_slider->value())/10;
        kx_b_k_LineEdit->setText(QString::number(tempk,10,2));

        double tempb = double(kx_b_b_slider->value())/10;
        kx_b_b_LineEdit->setText(QString::number(tempb,10,2));

        convertScaleAbs(srcImage,dstImage,tempk,tempb);
        dstlabel_show(srcImage,dstImage);
    };
    kx_b_b_slider->setStyleSheet("background-color:black;color:white");
    kx_b_k_slider->setStyleSheet("background-color:black;color:white");
    kx_b_b_LineEdit->setStyleSheet("background-color:black;color:white");
    kx_b_k_LineEdit->setStyleSheet("background-color:black;color:white");
    connect(kx_b_b_slider, &QAbstractSlider::valueChanged, fun);
    connect(kx_b_k_slider, &QAbstractSlider::valueChanged, fun);
    fun();
}

//透视变换（常用于图片矫正）
void MainWindow::on_action_toushibianhuan_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(!control_widget->isHidden())
    {
        control_widget->hide();
    }

    action = MainWindow::selection::toushibianhuan;//仿射变换
    src_copy_for_fangshe.release();
    srcImage.copyTo(src_copy_for_fangshe);
    point_num = 0;
    point_list.clear();
    if(dstImage.empty())
    {
        srcImage.copyTo(dstImage);
        dstlabel_show(srcImage,dstImage);
    }
}

//泊松融合(未完成）
void MainWindow::on_actionPoissonFusion_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QLabel * show_select_img_path = new QLabel(center_widget_in_dock);
    show_select_img_path->setWordWrap(true);
    show_select_img_path->setFixedWidth(300);
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::black);
    show_select_img_path->setPalette(palette);

    QPushButton * select_img = new QPushButton("选择要和源图融合的图片",center_widget_in_dock);
    select_img->setStyleSheet(QString("QPushButton{color:white;border-radius:5px;background:black;font-size:22px}"
                                      "QPushButton:hover{background:black;}"
                                      "QPushButton:pressed{background:black;}"));
                              //.arg(colorMain).arg(colorHover).arg(colorPressed));
    select_img->setFixedSize(300,36);

    QComboBox * combox = new QComboBox(center_widget_in_dock);
    combox->addItem(QString("NORMAL_CLONE"),cv::NORMAL_CLONE);
    combox->addItem(QString("MIXED_CLONE"),cv::MIXED_CLONE);
    combox->addItem(QString("MONOCHROME_TRANSFER"),cv::MONOCHROME_TRANSFER);
    combox->setView(new QListView());
    combox->setStyleSheet("background:black;color:white");

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(select_img,0,0);
    grildlayout->addWidget(show_select_img_path,0,1);
    grildLayoutAddWidgetLable("融合方式：",0,2);
    grildlayout->addWidget(combox,0,3);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignLeft);
    center_widget_in_dock->setLayout(grildlayout);

    seamlessRoi.release();
    connect(select_img,&QPushButton::clicked,[show_select_img_path,this]
    {
        QString surfFileName = QFileDialog::getOpenFileName(this,
                                                            QString("打开图片文件"),
                                                            openfilepath.isNull() ? "/" : openfilepath, //QDir::currentPath()
                                                            QString("Image File(*.bmp *.jpg *.jpeg *.png)"));

        if(surfFileName.isEmpty())
        {
            return;
        }

        show_select_img_path->setText(surfFileName);

        cv::Mat srcImage2 = cv::imread(QTextCodec::codecForName("gb18030")->fromUnicode(surfFileName).data());

        //将BGR转换为RGB，方便操作习惯
        cv::cvtColor(srcImage2,srcImage2,cv::COLOR_BGR2RGB);
        img = QImage((const unsigned char*)(srcImage2.data),
                     srcImage2.cols,
                     srcImage2.rows,
                     srcImage2.cols*srcImage2.channels(),
                     QImage::Format_RGB888);
        ui->label_src2->clear();

        //显示图像到QLabel控件
        ui->label_src2->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src2)));
        if(srcImage2.rows > srcImage2.rows * 0.8 || srcImage2.cols > srcImage2.cols * 0.8)
        {
            return;
        }
        else
        {
            seamlessRoi = srcImage2;
            action = MainWindow::selection::poissonFusion;
        }
    });
}

//Sobel算子
void MainWindow::on_actionSobel_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * sobel_fangxiang = new QComboBox(center_widget_in_dock);
    sobel_fangxiang->addItem(QString("梯度方向：dx"),1);
    sobel_fangxiang->addItem(QString("梯度方向：dy"),2);
    sobel_fangxiang->addItem(QString("梯度方向：dx & dy"),3);
    sobel_fangxiang->setView(new QListView);

    QComboBox * sobel_ksize = new QComboBox(center_widget_in_dock);
    sobel_ksize->setView(new QListView);
    for(int i = 1;i <= 20;i += 2)
    {
        sobel_ksize->addItem(QString("邻域范围：%1").arg(i),i);
    }

    QLabel * sobel_suofangyinzi_show = new QLabel(QString("1.00"),center_widget_in_dock);
    sobel_suofangyinzi_show->setStyleSheet("background-color:white");

    QSlider * sobel_suofangyinzi_Slider = new QSlider(center_widget_in_dock);
    sobel_suofangyinzi_Slider->setOrientation(Qt::Horizontal);
    sobel_suofangyinzi_Slider->setMinimum(1);
    sobel_suofangyinzi_Slider->setMaximum(100);
    sobel_suofangyinzi_Slider->setSingleStep(10);
    sobel_suofangyinzi_Slider->setTickInterval(5);
    sobel_suofangyinzi_Slider->setTickPosition(QSlider::TicksAbove);
    sobel_suofangyinzi_Slider->setValue(10);//

    QSpinBox * sobel_lianagdu_SpinBox = new QSpinBox(center_widget_in_dock);
    sobel_lianagdu_SpinBox->setMinimum(-100);
    sobel_lianagdu_SpinBox->setMaximum(100);
    sobel_lianagdu_SpinBox->setValue(0);//

    QSlider * sobel_lianagdu_Slider = new QSlider(center_widget_in_dock);
    sobel_lianagdu_Slider->setOrientation(Qt::Horizontal);
    sobel_lianagdu_Slider->setMinimum(-100);
    sobel_lianagdu_Slider->setMaximum(100);
    sobel_lianagdu_Slider->setSingleStep(10);
    sobel_lianagdu_Slider->setTickInterval(10);
    sobel_lianagdu_Slider->setTickPosition(QSlider::TicksAbove);
    sobel_lianagdu_Slider->setValue(0);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(sobel_fangxiang,0,0);
    grildlayout->addWidget(sobel_ksize,0,1);
    grildLayoutAddWidgetLable("设置亮度：",1,0);
    grildlayout->addWidget(sobel_lianagdu_SpinBox,1,1);
    grildlayout->addWidget(sobel_lianagdu_Slider,1,2,1,20);
    grildLayoutAddWidgetLable("缩放因子：",2,0);
    grildlayout->addWidget(sobel_suofangyinzi_show,2,1);
    grildlayout->addWidget(sobel_suofangyinzi_Slider,2,2,1,20);
    grildlayout->setContentsMargins(20,20,20,20);
    center_widget_in_dock->setLayout(grildlayout);

    connect(sobel_lianagdu_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), sobel_lianagdu_Slider, &QAbstractSlider::setValue);
    connect(sobel_lianagdu_Slider, &QAbstractSlider::valueChanged, sobel_lianagdu_SpinBox, &QSpinBox::setValue);

    auto fun = [=]
    {
        double temp = double(sobel_suofangyinzi_Slider->value())/10;
        sobel_suofangyinzi_show->setText(QString::number(temp,10,2));

        int dx = 0,dy = 0;
        int sobel_fangxiangvarint = sobel_fangxiang->currentData(Qt::UserRole).toInt();

        if(sobel_fangxiangvarint == 1)
        {
            dx = 1;
            dy = 0;
        }
        if(sobel_fangxiangvarint == 2)
        {
            dx = 0;
            dy = 1;
        }
        if(sobel_fangxiangvarint == 3)
        {
            dx = 1;
            dy = 1;
        }

        Sobel(srcImage,
              dstImage,
              srcImage.depth(),
              dx,
              dy,
              sobel_ksize->currentData(Qt::UserRole).toInt(),
              temp,
              sobel_lianagdu_SpinBox->value());
        dstlabel_show(srcImage,dstImage);
    };
    sobel_ksize->setStyleSheet("background-color:black;color:white");
    sobel_fangxiang->setStyleSheet("background-color:black;color:white");
    sobel_lianagdu_Slider->setStyleSheet("background-color:black;color:white");
    sobel_lianagdu_SpinBox->setStyleSheet("background-color:black;color:white");
    sobel_suofangyinzi_show->setStyleSheet("background-color:black;color:white");
    sobel_suofangyinzi_Slider->setStyleSheet("background-color:black;color:white");
    connect(sobel_suofangyinzi_Slider, &QAbstractSlider::valueChanged, fun);
    connect(sobel_ksize, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    connect(sobel_fangxiang, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    connect(sobel_lianagdu_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(sobel_lianagdu_Slider, &QAbstractSlider::valueChanged, fun);
    fun();
}

//Canny算子
void MainWindow::on_actionCanny_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QSpinBox * canny_SpinBox_l = new QSpinBox(center_widget_in_dock);
    canny_SpinBox_l->setMinimum(1);
    canny_SpinBox_l->setMaximum(254);
    canny_SpinBox_l->setValue(1);
    canny_SpinBox_l->setFixedHeight(36);

    QSlider * canny_slider_l = new QSlider(center_widget_in_dock);
    canny_slider_l->setOrientation(Qt::Horizontal);
    canny_slider_l->setMinimum(1);
    canny_slider_l->setMaximum(254);
    canny_slider_l->setSingleStep(10);
    canny_slider_l->setTickInterval(10);
    canny_slider_l->setTickPosition(QSlider::TicksAbove);
    canny_slider_l->setValue(1);
    canny_slider_l->setFixedHeight(36);

    QSpinBox * canny_SpinBox_h = new QSpinBox(center_widget_in_dock);
    canny_SpinBox_h->setMinimum(2);
    canny_SpinBox_h->setMaximum(1000);
    canny_SpinBox_h->setValue(2);
    canny_SpinBox_h->setFixedHeight(36);

    QSlider * canny_slider_h = new QSlider(center_widget_in_dock);
    canny_slider_h->setOrientation(Qt::Horizontal);
    canny_slider_h->setMinimum(2);
    canny_slider_h->setMaximum(1000);
    canny_slider_h->setSingleStep(20);
    canny_slider_h->setTickInterval(40);
    canny_slider_h->setTickPosition(QSlider::TicksAbove);
    canny_slider_h->setValue(2);
    canny_slider_h->setFixedHeight(36);

    QComboBox * canny_ksize = new QComboBox(center_widget_in_dock);
    canny_ksize->addItem(QString("核尺寸：3"),3);
    canny_ksize->addItem(QString("核尺寸：5"),5);
    canny_ksize->addItem(QString("核尺寸：7"),7);
    canny_ksize->setView(new QListView);
    canny_ksize->setFixedHeight(36);

    QComboBox * L2gradient = new QComboBox(center_widget_in_dock);
    L2gradient->addItem(QString("梯度强度：L1范数"),0);
    L2gradient->addItem(QString("梯度强度：L2范数"),1);
    L2gradient->setView(new QListView);
    L2gradient->setFixedHeight(36);

    QLabel * show_input_thresh2 = new QLabel("高阈值：2~1000：");

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(canny_ksize,0,0);
    grildlayout->addWidget(L2gradient,0,1);
    grildLayoutAddWidgetLable("低阈值：1~254：",1,0);
    grildlayout->addWidget(canny_SpinBox_l,1,1);
    grildlayout->addWidget(canny_slider_l,1,2);
    grildlayout->addWidget(show_input_thresh2,2,0);
    grildlayout->addWidget(canny_SpinBox_h,2,1);
    grildlayout->addWidget(canny_slider_h,2,2);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        int lowthreshint = canny_SpinBox_l->text().toInt();
        int threshdif = lowthreshint + 1;

        show_input_thresh2->setText(QString("高阈值：%1~1000：").arg(threshdif));

        canny_slider_h->setMinimum(threshdif);
        canny_SpinBox_h->setMinimum(threshdif);

        bool isL2gradient = false;
        if(L2gradient->currentData(Qt::UserRole).toInt() == 0)
        {
            isL2gradient = false;
        }
        else
        {
            isL2gradient = true;
        }

        cv::Mat srctemp;
        cvtColor(srcImage,srctemp,cv::COLOR_BGR2GRAY);
        Canny(srctemp,
              dstImage,
              lowthreshint,
              canny_SpinBox_h->text().toInt(),
              canny_ksize->currentData(Qt::UserRole).toInt(),
              isL2gradient);
        dstlabel_indexed8_show(dstImage);
    };
    canny_ksize->setStyleSheet("background-color:black;color:white");
    canny_slider_h->setStyleSheet("background-color:black;color:white");
    canny_slider_l->setStyleSheet("background-color:black;color:white");
    canny_SpinBox_h->setStyleSheet("background-color:black;color:white");
    canny_SpinBox_l->setStyleSheet("background-color:black;color:white");
    canny_ksize_lunkuojiance->setStyleSheet("background-color:black;color:white");
    connect(canny_ksize, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    connect(L2gradient, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    connect(canny_slider_l, &QAbstractSlider::valueChanged, canny_SpinBox_l, &QSpinBox::setValue);
    connect(canny_SpinBox_l, QOverload<int>::of(&QSpinBox::valueChanged), canny_slider_l, &QAbstractSlider::setValue);

    connect(canny_slider_l, &QAbstractSlider::valueChanged, fun);
    connect(canny_SpinBox_l,QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(canny_slider_h, &QAbstractSlider::valueChanged, canny_SpinBox_h, &QSpinBox::setValue);
    connect(canny_SpinBox_h, QOverload<int>::of(&QSpinBox::valueChanged), canny_slider_h, &QAbstractSlider::setValue);

    connect(canny_slider_h, &QAbstractSlider::valueChanged, fun);
    connect(canny_SpinBox_h, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    fun();
}


//Laplacian算子
void MainWindow::on_actionLaplacian_triggered()
{
    if (srcImage.empty())
    {
        //show_info("请先打开一张图片",Qt::AlignCenter);
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * Laplacian_ksize = new QComboBox(center_widget_in_dock);
    for(int i = 1;i <= 20;i += 2)
    {
        Laplacian_ksize->addItem(QString("核尺寸：%1 × %1").arg(i),i);
    }
    Laplacian_ksize->setView(new QListView);

    QLineEdit * Laplacian_LineEdit = new QLineEdit(center_widget_in_dock);
    Laplacian_LineEdit->setFocusPolicy(Qt::NoFocus);//无法获得焦点，即无法编辑
    Laplacian_LineEdit->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);

    QSlider * Laplacian_suofangyinzi_Slider = new QSlider(center_widget_in_dock);
    Laplacian_suofangyinzi_Slider->setOrientation(Qt::Horizontal);
    Laplacian_suofangyinzi_Slider->setMinimum(1);
    Laplacian_suofangyinzi_Slider->setMaximum(100);
    Laplacian_suofangyinzi_Slider->setSingleStep(10);
    Laplacian_suofangyinzi_Slider->setTickInterval(5);
    Laplacian_suofangyinzi_Slider->setTickPosition(QSlider::TicksAbove);
    Laplacian_suofangyinzi_Slider->setValue(10);//

    QSpinBox * Laplacian_lianagdu_SpinBox = new QSpinBox(center_widget_in_dock);
    Laplacian_lianagdu_SpinBox->setMinimum(-100);
    Laplacian_lianagdu_SpinBox->setMaximum(100);
    Laplacian_lianagdu_SpinBox->setValue(0);//

    QSlider * Laplacian_lianagdu_Slider = new QSlider(center_widget_in_dock);
    Laplacian_lianagdu_Slider->setOrientation(Qt::Horizontal);
    Laplacian_lianagdu_Slider->setMinimum(-100);
    Laplacian_lianagdu_Slider->setMaximum(100);
    Laplacian_lianagdu_Slider->setSingleStep(10);
    Laplacian_lianagdu_Slider->setTickInterval(10);
    Laplacian_lianagdu_Slider->setTickPosition(QSlider::TicksAbove);
    Laplacian_lianagdu_Slider->setValue(0);


    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(Laplacian_ksize,0,0);
    grildLayoutAddWidgetLable("设置亮度：",1,0);
    grildlayout->addWidget(Laplacian_lianagdu_SpinBox,1,1);
    grildlayout->addWidget(Laplacian_lianagdu_Slider,1,2,1,20);
    grildLayoutAddWidgetLable("缩放因子：",2,0);
    grildlayout->addWidget(Laplacian_LineEdit,2,1);
    grildlayout->addWidget(Laplacian_suofangyinzi_Slider,2,2,1,20);
    grildlayout->setContentsMargins(20,20,20,20);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        double temp = double(Laplacian_suofangyinzi_Slider->value())/10;
        Laplacian_LineEdit->setText(QString::number(temp,10,2));

        Laplacian(srcImage,
                  dstImage,
                  srcImage.depth(),
                  Laplacian_ksize->currentData(Qt::UserRole).toInt(),
                  temp,
                  Laplacian_lianagdu_SpinBox->text().toInt());
        dstlabel_show(srcImage,dstImage);
    };
    Laplacian_ksize->setStyleSheet("background-color:black;color:white");
    Laplacian_LineEdit->setStyleSheet("background-color:black;color:white");
    Laplacian_lianagdu_Slider->setStyleSheet("background-color:black;color:white");
    Laplacian_lianagdu_SpinBox->setStyleSheet("background-color:black;color:white");
    Laplacian_suofangyinzi_Slider->setStyleSheet("background-color:black;color:white");
    connect(Laplacian_suofangyinzi_Slider, &QAbstractSlider::valueChanged, fun);
    connect(Laplacian_ksize, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);

    connect(Laplacian_lianagdu_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Laplacian_lianagdu_Slider, &QAbstractSlider::setValue);
    connect(Laplacian_lianagdu_Slider, &QAbstractSlider::valueChanged, Laplacian_lianagdu_SpinBox, &QSpinBox::setValue);

    connect(Laplacian_lianagdu_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(Laplacian_lianagdu_Slider, &QAbstractSlider::valueChanged, fun);
    fun();
}

//Scharr算子
void MainWindow::on_actionScharr_triggered()
{
    if (srcImage.empty())
    {
        //show_info("请先打开一张图片",Qt::AlignCenter);
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QComboBox * Scharr_fangxiang = new QComboBox(center_widget_in_dock);
    Scharr_fangxiang->addItem(QString("梯度方向：dx"),1);
    Scharr_fangxiang->addItem(QString("梯度方向：dy"),2);
    Scharr_fangxiang->setView(new QListView);

    QLineEdit * Scharr_suofangyinzi_LineEdit = new QLineEdit(center_widget_in_dock);
    Scharr_suofangyinzi_LineEdit->setFocusPolicy(Qt::NoFocus);//无法获得焦点，即无法编辑
    Scharr_suofangyinzi_LineEdit->setSizePolicy(QSizePolicy::Preferred,QSizePolicy::Preferred);

    QSlider * Scharr_suofangyinzi_Slider = new QSlider(center_widget_in_dock);
    Scharr_suofangyinzi_Slider->setOrientation(Qt::Horizontal);
    Scharr_suofangyinzi_Slider->setMinimum(1);
    Scharr_suofangyinzi_Slider->setMaximum(100);
    Scharr_suofangyinzi_Slider->setSingleStep(10);
    Scharr_suofangyinzi_Slider->setTickInterval(5);
    Scharr_suofangyinzi_Slider->setTickPosition(QSlider::TicksAbove);
    Scharr_suofangyinzi_Slider->setValue(10);//

    QSpinBox * Scharr_lianagdu_SpinBox = new QSpinBox(center_widget_in_dock);
    Scharr_lianagdu_SpinBox->setMinimum(-100);
    Scharr_lianagdu_SpinBox->setMaximum(100);
    Scharr_lianagdu_SpinBox->setValue(0);//

    QSlider * Scharr_lianagdu_Slider = new QSlider(center_widget_in_dock);
    Scharr_lianagdu_Slider->setOrientation(Qt::Horizontal);
    Scharr_lianagdu_Slider->setMinimum(-100);
    Scharr_lianagdu_Slider->setMaximum(100);
    Scharr_lianagdu_Slider->setSingleStep(10);
    Scharr_lianagdu_Slider->setTickInterval(10);
    Scharr_lianagdu_Slider->setTickPosition(QSlider::TicksAbove);
    Scharr_lianagdu_Slider->setValue(0);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(Scharr_fangxiang,0,0);
    grildLayoutAddWidgetLable("设置亮度：",1,0);
    grildlayout->addWidget(Scharr_lianagdu_SpinBox,1,1);
    grildlayout->addWidget(Scharr_lianagdu_Slider,1,2,1,20);
    grildLayoutAddWidgetLable("缩放因子：",2,0);
    grildlayout->addWidget(Scharr_suofangyinzi_LineEdit,2,1);
    grildlayout->addWidget(Scharr_suofangyinzi_Slider,2,2,1,20);
    grildlayout->setContentsMargins(20,20,20,20);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        double temp = double(Scharr_suofangyinzi_Slider->value())/10;
        Scharr_suofangyinzi_LineEdit->setText(QString::number(temp,10,2));

        int dx,dy;
        if(Scharr_fangxiang->currentData(Qt::UserRole).toInt() == 1)
        {
            dx = 1;
            dy = 0;
        }
        else
        {
            dx = 0;
            dy = 1;
        }

        Scharr(srcImage,
               dstImage,
               srcImage.depth(),
               dx,
               dy,
               temp,
               Scharr_lianagdu_SpinBox->text().toInt());
        dstlabel_show(srcImage,dstImage);
    };
    Scharr_fangxiang->setStyleSheet("background-color:black;color:white");
    Scharr_lianagdu_Slider->setStyleSheet("background-color:black;color:white");
    Scharr_lianagdu_SpinBox->setStyleSheet("background-color:black;color:white");
    Scharr_suofangyinzi_Slider->setStyleSheet("background-color:black;color:white");
    Scharr_suofangyinzi_LineEdit->setStyleSheet("background-color:black;color:white");
    connect(Scharr_lianagdu_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Scharr_lianagdu_Slider, &QAbstractSlider::setValue);
    connect(Scharr_lianagdu_Slider, &QAbstractSlider::valueChanged, Scharr_lianagdu_SpinBox, &QSpinBox::setValue);
    connect(Scharr_lianagdu_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(Scharr_lianagdu_Slider, &QAbstractSlider::valueChanged, fun);
    connect(Scharr_suofangyinzi_Slider, &QAbstractSlider::valueChanged, fun);
    connect(Scharr_fangxiang, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    fun();
}

//轮廓检测 action
void MainWindow::on_action_lunkuojiance_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    canny_ksize_lunkuojiance = new QComboBox(center_widget_in_dock);
    canny_ksize_lunkuojiance->addItem(QString("Canny算子核尺寸：3"),3);
    canny_ksize_lunkuojiance->addItem(QString("Canny算子核尺寸：5"),5);
    canny_ksize_lunkuojiance->addItem(QString("Canny算子核尺寸：7"),7);
    canny_ksize_lunkuojiance->setView(new QListView());
    connect(canny_ksize_lunkuojiance, QOverload<int>::of(&QComboBox::currentIndexChanged),this, &MainWindow::lunkuojiance_process);

    L2gradient_lunkuojiance = new QComboBox(center_widget_in_dock);
    L2gradient_lunkuojiance->addItem(QString("Canny算子梯度强度：L1范数"),0);
    L2gradient_lunkuojiance->addItem(QString("Canny算子梯度强度：L2范数"),1);
    L2gradient_lunkuojiance->setView(new QListView());
    connect(L2gradient_lunkuojiance, QOverload<int>::of(&QComboBox::currentIndexChanged),this, &MainWindow::lunkuojiance_process);

    lunkuojiance_SpinBox_l = new QSpinBox(center_widget_in_dock);
    lunkuojiance_SpinBox_l->setMinimum(1);
    lunkuojiance_SpinBox_l->setMaximum(254);
    lunkuojiance_SpinBox_l->setValue(150);//

    lunkuojiance_slider_l = new QSlider(center_widget_in_dock);
    lunkuojiance_slider_l->setOrientation(Qt::Horizontal);
    lunkuojiance_slider_l->setMinimum(1);
    lunkuojiance_slider_l->setMaximum(254);
    lunkuojiance_slider_l->setSingleStep(10);
    lunkuojiance_slider_l->setTickInterval(10);
    lunkuojiance_slider_l->setTickPosition(QSlider::TicksAbove);
    lunkuojiance_slider_l->setValue(150);//

    connect(lunkuojiance_slider_l, &QAbstractSlider::valueChanged, lunkuojiance_SpinBox_l, &QSpinBox::setValue);
    connect(lunkuojiance_SpinBox_l, QOverload<int>::of(&QSpinBox::valueChanged), lunkuojiance_slider_l, &QAbstractSlider::setValue);

    connect(lunkuojiance_slider_l, &QAbstractSlider::valueChanged, this, &MainWindow::lunkuojiance_process);
    connect(lunkuojiance_SpinBox_l, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::lunkuojiance_process);

    lunkuojiance_SpinBox_h = new QSpinBox(center_widget_in_dock);
    lunkuojiance_SpinBox_h->setMinimum(2);
    lunkuojiance_SpinBox_h->setMaximum(1000);
    lunkuojiance_SpinBox_h->setValue(180);//

    lunkuojiance_slider_h = new QSlider(center_widget_in_dock);
    lunkuojiance_slider_h->setOrientation(Qt::Horizontal);
    lunkuojiance_slider_h->setMinimum(2);
    lunkuojiance_slider_h->setMaximum(1000);
    lunkuojiance_slider_h->setSingleStep(20);
    lunkuojiance_slider_h->setTickInterval(20);
    lunkuojiance_slider_h->setTickPosition(QSlider::TicksAbove);
    lunkuojiance_slider_h->setValue(180);//

    connect(lunkuojiance_slider_h, &QAbstractSlider::valueChanged, lunkuojiance_SpinBox_h, &QSpinBox::setValue);
    connect(lunkuojiance_SpinBox_h, QOverload<int>::of(&QSpinBox::valueChanged), lunkuojiance_slider_h, &QAbstractSlider::setValue);

    connect(lunkuojiance_slider_h, &QAbstractSlider::valueChanged, this, &MainWindow::lunkuojiance_process);
    connect(lunkuojiance_SpinBox_h, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::lunkuojiance_process);

    lunkuojiance_RETR_QComboBox = new QComboBox(center_widget_in_dock);
    lunkuojiance_RETR_QComboBox->addItem(QString("RETR_EXTERNAL"),cv::RETR_EXTERNAL);
    lunkuojiance_RETR_QComboBox->addItem(QString("RETR_LIST"),cv::RETR_LIST);
    lunkuojiance_RETR_QComboBox->addItem(QString("RETR_CCOMP"),cv::RETR_CCOMP);
    lunkuojiance_RETR_QComboBox->addItem(QString("RETR_TREE"),cv::RETR_TREE);
    lunkuojiance_RETR_QComboBox->setView(new QListView());
    connect(lunkuojiance_RETR_QComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),this, &MainWindow::lunkuojiance_process);

    lunkuojiance_APPROX_QComboBox = new QComboBox(center_widget_in_dock);
    lunkuojiance_APPROX_QComboBox->addItem(QString("CHAIN_APPROX_NONE"),cv::CHAIN_APPROX_NONE);
    lunkuojiance_APPROX_QComboBox->addItem(QString("CHAIN_APPROX_SIMPLE"),cv::CHAIN_APPROX_SIMPLE);
    lunkuojiance_APPROX_QComboBox->addItem(QString("CHAIN_APPROX_TC89_L1"),cv::CHAIN_APPROX_TC89_L1);
    lunkuojiance_APPROX_QComboBox->addItem(QString("CHAIN_APPROX_TC89_KCOS"),cv::CHAIN_APPROX_TC89_KCOS);
    lunkuojiance_APPROX_QComboBox->setView(new QListView());
    connect(lunkuojiance_APPROX_QComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),this, &MainWindow::lunkuojiance_process);

    lunkuojiance_color_QComboBox = new QComboBox(center_widget_in_dock);
    lunkuojiance_color_QComboBox->addItem(QString("黑白轮廓"),0);
    lunkuojiance_color_QComboBox->addItem(QString("彩色轮廓"),1);
    lunkuojiance_color_QComboBox->addItem(QString("黑白轮廓色，彩色绘制色"),2);
    lunkuojiance_color_QComboBox->setView(new QListView());
    connect(lunkuojiance_color_QComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),this, &MainWindow::lunkuojiance_process);

    lunkuojiance_lunkuocuxi_QComboBox = new QComboBox(center_widget_in_dock);
    lunkuojiance_lunkuocuxi_QComboBox->addItem(QString("轮廓线粗细：1"),1);
    lunkuojiance_lunkuocuxi_QComboBox->addItem(QString("轮廓线粗细：2"),2);
    lunkuojiance_lunkuocuxi_QComboBox->addItem(QString("轮廓线粗细：3"),3);
    lunkuojiance_lunkuocuxi_QComboBox->addItem(QString("轮廓填充"),-1);
    lunkuojiance_lunkuocuxi_QComboBox->setView(new QListView());
    connect(lunkuojiance_lunkuocuxi_QComboBox, QOverload<int>::of(&QComboBox::currentIndexChanged),this, &MainWindow::lunkuojiance_process);

    lunkuojiance_isdraw_ju = new QCheckBox(center_widget_in_dock);
    lunkuojiance_isdraw_ju->setText(QString("绘制图像矩"));
    connect(lunkuojiance_isdraw_ju, &QCheckBox::clicked, this, &MainWindow::lunkuojiance_process);

    lunkuojiance_draw_hull = new QCheckBox(center_widget_in_dock);
    lunkuojiance_draw_hull->setText(QString("绘制轮廓凸包"));
    connect(lunkuojiance_draw_hull, &QCheckBox::clicked, this, &MainWindow::lunkuojiance_process);

    lunkuojiance_draw_poly = new QCheckBox(center_widget_in_dock);
    lunkuojiance_draw_poly->setText(QString("绘制轮廓逼近多边形"));
    connect(lunkuojiance_draw_poly, &QCheckBox::clicked, this, &MainWindow::lunkuojiance_process);

    lunkuojiance_draw_in_src = new QCheckBox(center_widget_in_dock);
    lunkuojiance_draw_in_src->setText(QString("在源图上绘制"));
    connect(lunkuojiance_draw_in_src, &QCheckBox::clicked, this, &MainWindow::lunkuojiance_process);

    lunkuojiance_draw_rectangle = new QCheckBox(center_widget_in_dock);
    lunkuojiance_draw_rectangle->setText(QString("绘制轮廓外接矩形"));
    connect(lunkuojiance_draw_rectangle, &QCheckBox::clicked, this, &MainWindow::lunkuojiance_process);

    lunkuojiance_draw_circle = new QCheckBox(center_widget_in_dock);
    lunkuojiance_draw_circle->setText(QString("绘制轮廓外接圆"));
    connect(lunkuojiance_draw_circle, &QCheckBox::clicked, this, &MainWindow::lunkuojiance_process);

    lunkuojiance_draw_AreaRect = new QCheckBox(center_widget_in_dock);
    lunkuojiance_draw_AreaRect->setText(QString("绘制轮廓可旋转外接矩形"));
    connect(lunkuojiance_draw_AreaRect, &QCheckBox::clicked, this, &MainWindow::lunkuojiance_process);

    lunkuojiance_draw_Ellipse = new QCheckBox(center_widget_in_dock);
    lunkuojiance_draw_Ellipse->setText(QString("绘制轮廓外接椭圆"));
    connect(lunkuojiance_draw_Ellipse, &QCheckBox::clicked, this, &MainWindow::lunkuojiance_process);

    show_input_lunkuojiance_thresh = new QLabel(QString("Canny算子高阈值(2~1000)："),center_widget_in_dock);

    lunkuojiance_mianji_shaixuan_SpinBox = new QSpinBox(center_widget_in_dock);
    lunkuojiance_mianji_shaixuan_SpinBox->setMinimum(1);
    lunkuojiance_mianji_shaixuan_SpinBox->setMaximum(10000);
    lunkuojiance_mianji_shaixuan_SpinBox->setValue(1);//

    lunkuojiance_mianji_shaixuan_slider = new QSlider(center_widget_in_dock);
    lunkuojiance_mianji_shaixuan_slider->setOrientation(Qt::Horizontal);
    lunkuojiance_mianji_shaixuan_slider->setMinimum(1);
    lunkuojiance_mianji_shaixuan_slider->setMaximum(10000);
    lunkuojiance_mianji_shaixuan_slider->setSingleStep(200);
    lunkuojiance_mianji_shaixuan_slider->setTickInterval(400);
    lunkuojiance_mianji_shaixuan_slider->setTickPosition(QSlider::TicksAbove);
    lunkuojiance_mianji_shaixuan_slider->setValue(1);//

    connect(lunkuojiance_mianji_shaixuan_slider, &QAbstractSlider::valueChanged, lunkuojiance_mianji_shaixuan_SpinBox, &QSpinBox::setValue);
    connect(lunkuojiance_mianji_shaixuan_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), lunkuojiance_mianji_shaixuan_slider, QAbstractSlider::setValue);

    connect(lunkuojiance_mianji_shaixuan_slider, &QAbstractSlider::valueChanged, this, &MainWindow::lunkuojiance_process);
    connect(lunkuojiance_mianji_shaixuan_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::lunkuojiance_process);

    lunkuojiance_slider_h->setStyleSheet("background-color:black;color:white");
    lunkuojiance_slider_l->setStyleSheet("background-color:black;color:white");
    lunkuojiance_SpinBox_h->setStyleSheet("background-color:black;color:white");
    lunkuojiance_SpinBox_l->setStyleSheet("background-color:black;color:white");
    lunkuojiance_draw_hull->setStyleSheet("background-color:black;color:white");
    lunkuojiance_draw_poly->setStyleSheet("background-color:black;color:white");
    lunkuojiance_isdraw_ju->setStyleSheet("background-color:black;color:white");
    lunkuojiance_draw_circle->setStyleSheet("background-color:black;color:white");
    lunkuojiance_draw_in_src->setStyleSheet("background-color:black;color:white");
    lunkuojiance_draw_Ellipse->setStyleSheet("background-color:black;color:white");
    lunkuojiance_draw_AreaRect->setStyleSheet("background-color:black;color:white");
    lunkuojiance_RETR_QComboBox->setStyleSheet("background-color:black;color:white");
    lunkuojiance_draw_rectangle->setStyleSheet("background-color:black;color:white");
    lunkuojiance_color_QComboBox->setStyleSheet("background-color:black;color:white");
    lunkuojiance_APPROX_QComboBox->setStyleSheet("background-color:black;color:white");
    lunkuojiance_lunkuocuxi_QComboBox->setStyleSheet("background-color:black;color:white");
    lunkuojiance_mianji_shaixuan_slider->setStyleSheet("background-color:black;color:white");
    lunkuojiance_mianji_shaixuan_SpinBox->setStyleSheet("background-color:black;color:white");
    canny_ksize_lunkuojiance->setStyleSheet("background-color:black;color:white");
    L2gradient_lunkuojiance->setStyleSheet("background-color:black;color:white");

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(canny_ksize_lunkuojiance,0,0);
    grildlayout->addWidget(L2gradient_lunkuojiance,0,1);
    QLabel* lable = new QLabel(" 注意：处理大图会比较耗时");
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::black);  // 设置文本颜色为黑
    lable->setPalette(palette);
    grildlayout->addWidget(lable, 0, 2,1,2);
    grildLayoutAddWidgetLable("Canny算子低阈值(1~254)：",1,0);
    grildlayout->addWidget(lunkuojiance_SpinBox_l,1,1);
    grildlayout->addWidget(lunkuojiance_slider_l,1,2,1,8);
    grildlayout->addWidget(show_input_lunkuojiance_thresh,2,0);
    grildlayout->addWidget(lunkuojiance_SpinBox_h,2,1);
    grildlayout->addWidget(lunkuojiance_slider_h,2,2,1,8);
    grildLayoutAddWidgetLable("轮廓按面积筛选：",3,0);
    grildlayout->addWidget(lunkuojiance_mianji_shaixuan_SpinBox,3,1);
    grildlayout->addWidget(lunkuojiance_mianji_shaixuan_slider,3,2,1,8);
    grildlayout->addWidget(lunkuojiance_RETR_QComboBox,4,0);
    grildlayout->addWidget(lunkuojiance_APPROX_QComboBox,4,1);
    grildlayout->addWidget(lunkuojiance_lunkuocuxi_QComboBox,4,2);
    grildlayout->addWidget(lunkuojiance_color_QComboBox,4,3);
    grildlayout->addWidget(lunkuojiance_isdraw_ju,5,0);
    grildlayout->addWidget(lunkuojiance_draw_hull,5,1);
    grildlayout->addWidget(lunkuojiance_draw_poly,5,2);
    grildlayout->addWidget(lunkuojiance_draw_in_src,5,3);
    grildlayout->addWidget(lunkuojiance_draw_rectangle,6,0);
    grildlayout->addWidget(lunkuojiance_draw_AreaRect,6,1);
    grildlayout->addWidget(lunkuojiance_draw_circle,6,2);
    grildlayout->addWidget(lunkuojiance_draw_Ellipse,6,3);
    grildlayout->setContentsMargins(20,20,20,20);

    center_widget_in_dock->setLayout(grildlayout);

    lunkuojiance_process();
}

//轮廓检测算法实现
void MainWindow::lunkuojiance_process()
{
    int cannylowthreshint = lunkuojiance_SpinBox_l->text().toInt();
    int cannythreshdif = cannylowthreshint + 1;

    show_input_lunkuojiance_thresh->setText(QString("Canny算子高阈值(%1~1000)：").arg(cannythreshdif));

    lunkuojiance_slider_h->setMinimum(cannythreshdif);
    lunkuojiance_SpinBox_h->setMinimum(cannythreshdif);

    int line_cuxi = lunkuojiance_lunkuocuxi_QComboBox->currentData(Qt::UserRole).toInt();

    bool isL2gradient = false;
    int l2gradientvarint = L2gradient_lunkuojiance->currentData(Qt::UserRole).toInt();
    if(l2gradientvarint == 0)
    {
        isL2gradient = false;
    }
    else
    {
        isL2gradient = true;
    }

    cv::Mat grayImage,cannygrayImage;
    cvtColor(srcImage,grayImage,cv::COLOR_BGR2GRAY);

    Canny(grayImage,
          cannygrayImage,
          cannylowthreshint,
          lunkuojiance_SpinBox_h->text().toInt(),
          canny_ksize_lunkuojiance->currentData(Qt::UserRole).toInt(),//核尺寸
          isL2gradient);

    std::vector<std::vector<cv::Point>> g_vContours_front;//筛选之前的轮廓
    std::vector<std::vector<cv::Point>> g_vContours;//筛选之后的轮廓
    std::vector<cv::Vec4i> g_vHierarchy;
    cv::Mat cannyfindContours;
    cannygrayImage.copyTo(cannyfindContours);

    findContours(cannyfindContours,
                 g_vContours_front,
                 g_vHierarchy,
                 lunkuojiance_RETR_QComboBox->currentData(Qt::UserRole).toInt(),
                 lunkuojiance_APPROX_QComboBox->currentData(Qt::UserRole).toInt());

    size_t size_int = g_vContours_front.size();
    int mianji = lunkuojiance_mianji_shaixuan_SpinBox->value();
    for (size_t i = 0; i < size_int; ++i)//轮廓筛选 面积小于阈值的剔除
    {
         if (fabs(contourArea(cv::Mat(g_vContours_front[i]))) > mianji)
         {
             g_vContours.push_back(g_vContours_front[i]);
         }
    }

    size_t size_saixuan = g_vContours.size();
    std::vector<cv::Moments> mu(size_saixuan);   //根据轮廓计算矩
    for (unsigned int i = 0; i < size_saixuan; ++i)
    {
        mu[i] = moments(g_vContours[i], false);
    }

    std::vector<cv::Point2f> mc(size_saixuan);  //根据轮廓计算中心矩
    for (unsigned int i = 0; i < size_saixuan; ++i)
    {
        mc[i] = cv::Point2f(static_cast<float>(mu[i].m10/mu[i].m00), static_cast<float>(mu[i].m01 / mu[i].m00));
    }

    std::vector<std::vector<cv::Point> > hull(size_saixuan);//轮廓凸包
    std::vector<std::vector<cv::Point> >contours_poly(size_saixuan);//轮廓外接多边形
    std::vector<cv::Rect>boundRect(size_saixuan);//轮廓外接矩形
    std::vector<cv::Point2f>center(size_saixuan);//轮廓外接圆-圆心
    std::vector<float>radius(size_saixuan);//轮廓外接圆-半径
    std::vector<cv::RotatedRect>minRect(size_saixuan);//轮廓外接可旋转矩形
    std::vector<cv::RotatedRect>minEllipse(size_saixuan);//轮廓外接椭圆

    bool hull_fangxiang = (rand()&1) == 1 ? true:false;//凸包方向随机
    for (unsigned int i = 0; i < size_saixuan; ++i)
    {
        convexHull(cv::Mat(g_vContours[i]),hull[i],hull_fangxiang);//凸包

        approxPolyDP(cv::Mat(g_vContours[i]), contours_poly[i], 3, true);//本轮廓的外接多边形
        boundRect[i] = boundingRect(cv::Mat(contours_poly[i]));//从外接多边形获取外接矩形
        minEnclosingCircle((cv::Mat)contours_poly[i], center[i], radius[i]);//从外接多边形获取外接圆

        minRect[i] = minAreaRect(cv::Mat(g_vContours[i]));

        if (g_vContours[i].size() > 5)
        {
            minEllipse[i] = fitEllipse(cv::Mat(g_vContours[i]));//此函数要求轮廓至少有6个点
        }
    }

    cv::Scalar Contourscolor;
    dstImage = cv::Mat::zeros(cannyfindContours.size(), CV_8UC3);

    if(lunkuojiance_draw_in_src->isChecked())//在源图绘制
    {
        srcImage.copyTo(dstImage);
    }

    int huizhicolor = lunkuojiance_color_QComboBox->currentData(Qt::UserRole).toInt();
    for ( unsigned int i = 0; i < size_saixuan; ++i)
    {
        if(huizhicolor == 0)
        {
            Contourscolor = cv::Scalar(255,255,255);
        }
        else
        {
            Contourscolor = cv::Scalar(rand()&255,rand()&255,rand()&255);
        }

        //绘制轮廓
        if(huizhicolor == 2)
        {
            drawContours(dstImage,
                         g_vContours,
                         i,
                         cv::Scalar(255,255,255),
                         line_cuxi);
        }
        else
        {
            drawContours(dstImage,
                         g_vContours,
                         i,
                         Contourscolor,
                         line_cuxi);
        }

        //绘制矩
        if(lunkuojiance_isdraw_ju->isChecked())
        {
            circle(dstImage,
                   mc[i],
                   line_cuxi > 0 ? line_cuxi * 3 : 3,
                   Contourscolor,
                   -1);
        }

        //绘制凸包
        if(lunkuojiance_draw_hull->isChecked())
        {
            drawContours(dstImage,hull,i,Contourscolor,line_cuxi);
        }

        //绘制逼近多边形
        if(lunkuojiance_draw_poly->isChecked())
        {
            drawContours(dstImage, contours_poly, i, Contourscolor, line_cuxi);
        }

        //绘制外接矩形
        if(lunkuojiance_draw_rectangle->isChecked())
        {
            rectangle(dstImage, boundRect[i].tl(), boundRect[i].br(), Contourscolor, line_cuxi);
        }

        //绘制外接圆
        if(lunkuojiance_draw_circle->isChecked())
        {
            circle(dstImage, center[i], (int)radius[i], Contourscolor, line_cuxi);
        }

        //绘制可旋转外接矩形
        if(lunkuojiance_draw_AreaRect->isChecked())
        {
            cv::Point2f rect_Points[4];
            minRect[i].points(rect_Points);
            if(line_cuxi != -1)//非填充
            {
                for (int j = 0; j < 4; ++j)
                {
                    line(dstImage, rect_Points[j], rect_Points[(j + 1) % 4], Contourscolor, line_cuxi);
                }
            }
            else //填充
            {
                cv::Point rookPoints[1][4];
                for (int j = 0; j < 4; ++j)
                {
                    rookPoints[0][j] = rect_Points[j];
                }
                const cv::Point * ppt[1] = {rookPoints[0]};
                int npt[] = {4};
                fillPoly(dstImage, ppt, npt, 1, Contourscolor);//多边形
            }
        }

        //绘制外接椭圆
        if(lunkuojiance_draw_Ellipse->isChecked())
        {
            ellipse(dstImage, minEllipse[i], Contourscolor, line_cuxi);
        }
    }

    dstlabel_show(srcImage,dstImage);
}

//霍夫线变换
void MainWindow::on_action_huofu_line_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QSpinBox * Hough_canny_SpinBox_l = new QSpinBox(center_widget_in_dock);
    Hough_canny_SpinBox_l->setMinimum(1);
    Hough_canny_SpinBox_l->setMaximum(254);
    Hough_canny_SpinBox_l->setValue(50);

    QSlider * Hough_canny_slider_l = new QSlider(center_widget_in_dock);
    Hough_canny_slider_l->setOrientation(Qt::Horizontal);
    Hough_canny_slider_l->setMinimum(1);
    Hough_canny_slider_l->setMaximum(254);
    Hough_canny_slider_l->setSingleStep(10);
    Hough_canny_slider_l->setTickInterval(10);
    Hough_canny_slider_l->setTickPosition(QSlider::TicksAbove);
    Hough_canny_slider_l->setValue(50);

    QSpinBox * Hough_canny_SpinBox_h = new QSpinBox(center_widget_in_dock);
    Hough_canny_SpinBox_h->setMinimum(2);
    Hough_canny_SpinBox_h->setMaximum(1000);
    Hough_canny_SpinBox_h->setValue(200);

    QSlider * Hough_canny_slider_h = new QSlider(center_widget_in_dock);
    Hough_canny_slider_h->setOrientation(Qt::Horizontal);
    Hough_canny_slider_h->setMinimum(2);
    Hough_canny_slider_h->setMaximum(1000);
    Hough_canny_slider_h->setSingleStep(20);
    Hough_canny_slider_h->setTickInterval(40);
    Hough_canny_slider_h->setTickPosition(QSlider::TicksAbove);
    Hough_canny_slider_h->setValue(200);

    QSpinBox * Hough_line_SpinBox = new QSpinBox(center_widget_in_dock);
    Hough_line_SpinBox->setMinimum(1);
    Hough_line_SpinBox->setMaximum(255);
    Hough_line_SpinBox->setValue(200);

    QSlider * Hough_line_slider = new QSlider(center_widget_in_dock);
    Hough_line_slider->setOrientation(Qt::Horizontal);
    Hough_line_slider->setMinimum(1);
    Hough_line_slider->setMaximum(255);
    Hough_line_slider->setSingleStep(10);
    Hough_line_slider->setTickInterval(10);
    Hough_line_slider->setTickPosition(QSlider::TicksAbove);
    Hough_line_slider->setValue(200);

    QSpinBox * Hough_line_minlength_SpinBox = new QSpinBox(center_widget_in_dock);
    Hough_line_minlength_SpinBox->setMinimum(20);
    Hough_line_minlength_SpinBox->setMaximum(1000);
    Hough_line_minlength_SpinBox->setValue(50);

    QSlider * Hough_line_minlength_slider = new QSlider(center_widget_in_dock);
    Hough_line_minlength_slider->setOrientation(Qt::Horizontal);
    Hough_line_minlength_slider->setMinimum(20);
    Hough_line_minlength_slider->setMaximum(100);
    Hough_line_minlength_slider->setSingleStep(20);
    Hough_line_minlength_slider->setTickInterval(25);
    Hough_line_minlength_slider->setTickPosition(QSlider::TicksAbove);
    Hough_line_minlength_slider->setValue(50);

    QSpinBox * Hough_line_maxLineGap_SpinBox = new QSpinBox(center_widget_in_dock);
    Hough_line_maxLineGap_SpinBox->setMinimum(1);
    Hough_line_maxLineGap_SpinBox->setMaximum(100);
    Hough_line_maxLineGap_SpinBox->setValue(10);

    QSlider * Hough_line_maxLineGap_slider = new QSlider(center_widget_in_dock);
    Hough_line_maxLineGap_slider->setOrientation(Qt::Horizontal);
    Hough_line_maxLineGap_slider->setMinimum(1);
    Hough_line_maxLineGap_slider->setMaximum(100);
    Hough_line_maxLineGap_slider->setSingleStep(10);
    Hough_line_maxLineGap_slider->setTickInterval(10);
    Hough_line_maxLineGap_slider->setTickPosition(QSlider::TicksAbove);
    Hough_line_maxLineGap_slider->setValue(10);

    QSpinBox * Hough_line_thickness = new QSpinBox(center_widget_in_dock);
    Hough_line_thickness->setMinimum(1);
    Hough_line_thickness->setMaximum(12);
    Hough_line_thickness->setValue(2);

    QCheckBox * Hough_line_color = new QCheckBox(center_widget_in_dock);
    Hough_line_color->setText(QString("单色/多色"));

    QCheckBox * Hough_line_draw_in_src = new QCheckBox(center_widget_in_dock);
    Hough_line_draw_in_src->setText(QString("在源图上绘制直线"));

    QLabel * Hough_canny_show = new QLabel(QString("Canny算子高阈值：2~1000"),center_widget_in_dock);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("Canny算子低阈值：1~254",0,0);
    grildlayout->addWidget(Hough_canny_SpinBox_l,0,1);
    grildlayout->addWidget(Hough_canny_slider_l,0,2,1,8);
    grildlayout->addWidget(Hough_canny_show,1,0);
    grildlayout->addWidget(Hough_canny_SpinBox_h,1,1);
    grildlayout->addWidget(Hough_canny_slider_h,1,2,1,8);
    grildLayoutAddWidgetLable("线段检测阈值：",2,0);
    grildlayout->addWidget(Hough_line_SpinBox,2,1);
    grildlayout->addWidget(Hough_line_slider,2,2,1,8);
    grildLayoutAddWidgetLable("被检测出线段的最小长度：",3,0);
    grildlayout->addWidget(Hough_line_minlength_SpinBox,3,1);
    grildlayout->addWidget(Hough_line_minlength_slider,3,2,1,8);
    grildLayoutAddWidgetLable("允许连接两点间的最大距离",4,0);
    grildlayout->addWidget(Hough_line_maxLineGap_SpinBox,4,1);
    grildlayout->addWidget(Hough_line_maxLineGap_slider,4,2,1,8);
    grildLayoutAddWidgetLable("直线粗细：",5,0);
    grildlayout->addWidget(Hough_line_thickness,5,1);
    grildlayout->addWidget(Hough_line_color,5,2);
    grildlayout->addWidget(Hough_line_draw_in_src,5,3);
    grildlayout->setContentsMargins(20,20,20,20);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        int cannythreshdif = Hough_canny_SpinBox_l->text().toInt() + 1;
        Hough_canny_show->setText(QString("Canny算子高阈值(%1~1000)：").arg(cannythreshdif));
        Hough_canny_slider_h->setMinimum(cannythreshdif);
        Hough_canny_SpinBox_h->setMinimum(cannythreshdif);

        cv::Mat midImage;
        Canny(srcImage,
              midImage,
              Hough_canny_SpinBox_l->text().toInt(),
              Hough_canny_SpinBox_h->text().toInt(),
              3,
              true);
        cvtColor(midImage,dstImage, cv::COLOR_GRAY2BGR);

        std::vector<cv::Vec4i> lines;//定义一个矢量结构lines用于存放得到的线段矢量集合
        HoughLinesP(midImage,
                    lines,
                    1,
                    CV_PI/180,
                    Hough_line_SpinBox->text().toInt(),
                    Hough_line_minlength_SpinBox->text().toDouble(),
                    Hough_line_maxLineGap_SpinBox->text().toDouble());

        if(Hough_line_draw_in_src->isChecked())//选中则在源图绘制
        {
            srcImage.copyTo(dstImage);
        }

        cv::Scalar linescolor_single = cv::Scalar(rand()&255,rand()&255,rand()&255);
        cv::Scalar draw_color;
        cv::Vec4i l;
        size_t size = lines.size();
        int line_thickness = Hough_line_thickness->text().toInt();
        for(size_t i = 0; i < size; ++i)
        {
            l = lines[i];
            if(Hough_line_color->isChecked())
            {
                draw_color = cv::Scalar(rand()&255,rand()&255,rand()&255);//多色
            }
            else
            {
                draw_color = linescolor_single;
            }
            line(dstImage, cv::Point(l[0], l[1]), cv::Point(l[2], l[3]), draw_color, line_thickness, cv::LINE_AA);
        }

        dstlabel_show(srcImage,dstImage);
    };
    Hough_canny_show->setStyleSheet("background-color:black;color:white");
    Hough_line_color->setStyleSheet("background-color:black;color:white");
    Hough_line_slider->setStyleSheet("background-color:black;color:white");
    Hough_line_SpinBox->setStyleSheet("background-color:black;color:white");
    Hough_canny_slider_h->setStyleSheet("background-color:black;color:white");
    Hough_canny_slider_l->setStyleSheet("background-color:black;color:white");
    Hough_line_thickness->setStyleSheet("background-color:black;color:white");
    Hough_canny_SpinBox_h->setStyleSheet("background-color:black;color:white");
    Hough_canny_SpinBox_l->setStyleSheet("background-color:black;color:white");
    Hough_line_draw_in_src->setStyleSheet("background-color:black;color:white");
    Hough_line_minlength_slider->setStyleSheet("background-color:black;color:white");
    Hough_line_maxLineGap_slider->setStyleSheet("background-color:black;color:white");
    Hough_line_minlength_SpinBox->setStyleSheet("background-color:black;color:white");
    Hough_line_maxLineGap_SpinBox->setStyleSheet("background-color:black;color:white");

    connect(Hough_canny_slider_l, &QAbstractSlider::valueChanged, Hough_canny_SpinBox_l, &QSpinBox::setValue);
    connect(Hough_canny_SpinBox_l, QOverload<int>::of(&QSpinBox::valueChanged), Hough_canny_slider_l, &QAbstractSlider::setValue);

    connect(Hough_canny_SpinBox_l, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(Hough_canny_slider_l, &QAbstractSlider::valueChanged, fun);

    connect(Hough_canny_slider_h, &QAbstractSlider::valueChanged, Hough_canny_SpinBox_h, &QSpinBox::setValue);
    connect(Hough_canny_SpinBox_h, QOverload<int>::of(&QSpinBox::valueChanged), Hough_canny_slider_h, &QAbstractSlider::setValue);

    connect(Hough_canny_slider_h, &QAbstractSlider::valueChanged, fun);
    connect(Hough_canny_SpinBox_h, QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(Hough_line_slider, &QAbstractSlider::valueChanged, Hough_line_SpinBox, &QSpinBox::setValue);
    connect(Hough_line_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Hough_line_slider, &QAbstractSlider::setValue);

    connect(Hough_line_slider, &QAbstractSlider::valueChanged, fun);
    connect(Hough_line_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(Hough_line_minlength_slider, &QAbstractSlider::valueChanged, Hough_line_minlength_SpinBox, &QSpinBox::setValue);
    connect(Hough_line_minlength_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Hough_line_minlength_slider, &QAbstractSlider::setValue);

    connect(Hough_line_minlength_slider, &QAbstractSlider::valueChanged, fun);
    connect(Hough_line_minlength_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(Hough_line_maxLineGap_slider, &QAbstractSlider::valueChanged, Hough_line_maxLineGap_SpinBox, &QSpinBox::setValue);
    connect(Hough_line_maxLineGap_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Hough_line_maxLineGap_slider, &QAbstractSlider::setValue);

    connect(Hough_line_maxLineGap_slider, &QAbstractSlider::valueChanged, fun);
    connect(Hough_line_maxLineGap_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(Hough_line_draw_in_src, &QCheckBox::clicked, fun);
    connect(Hough_line_color, &QCheckBox::clicked, fun);
    connect(Hough_line_thickness, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    fun();
}

//霍夫圆变换
void MainWindow::on_action_huofu_yuan_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QSpinBox * Hough_circles_yuanxin_SpinBox = new QSpinBox(center_widget_in_dock);
    Hough_circles_yuanxin_SpinBox->setMinimum(1);
    Hough_circles_yuanxin_SpinBox->setMaximum(20);
    Hough_circles_yuanxin_SpinBox->setValue(5);//

    QSlider * Hough_circles_yuanxin_slider = new QSlider(center_widget_in_dock);
    Hough_circles_yuanxin_slider->setOrientation(Qt::Horizontal);
    Hough_circles_yuanxin_slider->setMinimum(1);
    Hough_circles_yuanxin_slider->setMaximum(20);
    Hough_circles_yuanxin_slider->setSingleStep(1);
    Hough_circles_yuanxin_slider->setTickInterval(1);
    Hough_circles_yuanxin_slider->setTickPosition(QSlider::TicksAbove);
    Hough_circles_yuanxin_slider->setValue(5);

    QSpinBox * Hough_circles_canny_SpinBox_h = new QSpinBox(center_widget_in_dock);
    Hough_circles_canny_SpinBox_h->setMinimum(2);
    Hough_circles_canny_SpinBox_h->setMaximum(600);
    Hough_circles_canny_SpinBox_h->setValue(300);

    QSlider * Hough_circles_canny_slider_h = new QSlider(center_widget_in_dock);
    Hough_circles_canny_slider_h->setOrientation(Qt::Horizontal);
    Hough_circles_canny_slider_h->setMinimum(2);
    Hough_circles_canny_slider_h->setMaximum(600);
    Hough_circles_canny_slider_h->setSingleStep(10);
    Hough_circles_canny_slider_h->setTickInterval(20);
    Hough_circles_canny_slider_h->setTickPosition(QSlider::TicksAbove);
    Hough_circles_canny_slider_h->setValue(300);

    QSpinBox * Hough_circles_point_SpinBox = new QSpinBox(center_widget_in_dock);
    Hough_circles_point_SpinBox->setMinimum(1);
    Hough_circles_point_SpinBox->setMaximum(255);
    Hough_circles_point_SpinBox->setValue(100);

    QSlider * Hough_circles_point_slider = new QSlider(center_widget_in_dock);
    Hough_circles_point_slider->setOrientation(Qt::Horizontal);
    Hough_circles_point_slider->setMinimum(1);
    Hough_circles_point_slider->setMaximum(255);
    Hough_circles_point_slider->setSingleStep(10);
    Hough_circles_point_slider->setTickInterval(10);
    Hough_circles_point_slider->setTickPosition(QSlider::TicksAbove);
    Hough_circles_point_slider->setValue(100);

    QSpinBox * Hough_minRadius_SpinBox = new QSpinBox(center_widget_in_dock);
    Hough_minRadius_SpinBox->setMinimum(0);
    Hough_minRadius_SpinBox->setMaximum(1000);
    Hough_minRadius_SpinBox->setValue(0);

    QSlider * Hough_minRadius_slider = new QSlider(center_widget_in_dock);
    Hough_minRadius_slider->setOrientation(Qt::Horizontal);
    Hough_minRadius_slider->setMinimum(0);
    Hough_minRadius_slider->setMaximum(1000);
    Hough_minRadius_slider->setSingleStep(50);
    Hough_minRadius_slider->setTickInterval(40);
    Hough_minRadius_slider->setTickPosition(QSlider::TicksAbove);
    Hough_minRadius_slider->setValue(0);

    QSpinBox * Hough_maxRadius_SpinBox = new QSpinBox(center_widget_in_dock);
    Hough_maxRadius_SpinBox->setMinimum(0);
    Hough_maxRadius_SpinBox->setMaximum(1000);
    Hough_maxRadius_SpinBox->setValue(0);

    QSlider * Hough_maxRadius_slider = new QSlider(center_widget_in_dock);
    Hough_maxRadius_slider->setOrientation(Qt::Horizontal);
    Hough_maxRadius_slider->setMinimum(0);
    Hough_maxRadius_slider->setMaximum(1000);
    Hough_maxRadius_slider->setSingleStep(50);
    Hough_maxRadius_slider->setTickInterval(40);
    Hough_maxRadius_slider->setTickPosition(QSlider::TicksAbove);
    Hough_maxRadius_slider->setValue(0);

    QSpinBox * Hough_circles_thickness = new QSpinBox(center_widget_in_dock);
    Hough_circles_thickness->setMinimum(-1);
    Hough_circles_thickness->setMaximum(12);
    Hough_circles_thickness->setValue(3);

    QCheckBox * Hough_Radius_draw_in_src = new QCheckBox("绘制半径               注意：处理大图会比较耗时",center_widget_in_dock);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("圆心间最小距离：",0,0);
    grildlayout->addWidget(Hough_circles_yuanxin_SpinBox,0,1);
    grildlayout->addWidget(Hough_circles_yuanxin_slider,0,2,1,8);
    grildLayoutAddWidgetLable("霍夫梯度法边缘检测Canny算子的高阈值：",1,0);
    grildlayout->addWidget(Hough_circles_canny_SpinBox_h,1,1);
    grildlayout->addWidget(Hough_circles_canny_slider_h,1,2,1,8);
    grildLayoutAddWidgetLable("霍夫梯度法检测圆心的累加器阈值：",2,0);
    grildlayout->addWidget(Hough_circles_point_SpinBox,2,1);
    grildlayout->addWidget(Hough_circles_point_slider,2,2,1,8);
    grildLayoutAddWidgetLable("最小半径：",3,0);
    grildlayout->addWidget(Hough_minRadius_SpinBox,3,1);
    grildlayout->addWidget(Hough_minRadius_slider,3,2,1,8);
    grildLayoutAddWidgetLable("最大半径：",4,0);
    grildlayout->addWidget(Hough_maxRadius_SpinBox,4,1);
    grildlayout->addWidget(Hough_maxRadius_slider,4,2,1,8);
    grildLayoutAddWidgetLable("圆线粗细：",5,0);
    grildlayout->addWidget(Hough_circles_thickness,5,1);
    grildlayout->addWidget(Hough_Radius_draw_in_src,5,2,1,3);
    grildlayout->setContentsMargins(20,20,20,20);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        cv::Mat midImage;
        cvtColor(srcImage,midImage, cv::COLOR_BGR2GRAY);
        medianBlur(midImage,midImage,3);

        std::vector<cv::Vec3f> circles;//Vec3f记录3个值 圆心坐标、半径
        HoughCircles(midImage,
                     circles,
                     cv::HOUGH_GRADIENT,//霍夫梯度法
                     1,
                     Hough_circles_yuanxin_SpinBox->text().toDouble(),
                     Hough_circles_canny_SpinBox_h->text().toDouble(),
                     Hough_circles_point_SpinBox->text().toDouble(), //大于这个值才被当做圆心（猜测）
                     Hough_minRadius_SpinBox->text().toInt(),
                     Hough_maxRadius_SpinBox->text().toInt());

        srcImage.copyTo(dstImage);
        cv::Scalar linescolor;

        size_t size = circles.size();
        cv::Point center;
        int radius,angle,x1,y1;
        for(size_t i = 0; i < size; ++i)
        {
            linescolor = cv::Scalar(rand()&255,rand()&255,rand()&255);
            center = cv::Point(cvRound(circles[i][0]), cvRound(circles[i][1]));
            radius = cvRound(circles[i][2]);
            //绘制圆轮廓
            circle(dstImage,
                   center,
                   radius,
                   linescolor,
                   Hough_circles_thickness->text().toInt(),
                   8);
            //绘制圆心
            circle(dstImage, center, 5, cv::Scalar(255, 0, 0), -1, 8);

            if(Hough_Radius_draw_in_src->checkState() == Qt::Checked)//选中绘制半径
            {
                angle = qrand() % 360;//随机角度
                //根据角度、半径、圆心求另一点的坐标
                x1 = int(center.x + radius * cos(angle * CV_PI/180));
                y1 = int(center.y + radius * sin(angle * CV_PI/180));
                if(Hough_circles_thickness->text().toInt() == -1)
                {
                    line(dstImage, center, cv::Point(x1,y1), cv::Scalar(rand()&255,rand()&255,rand()&255), 2);
                }
                else
                {
                    line(dstImage, center, cv::Point(x1,y1), linescolor, Hough_circles_thickness->text().toInt());
                }
            }
        }
        dstlabel_show(srcImage,dstImage);
    };
    Hough_maxRadius_slider->setStyleSheet("background-color:black;color:white");
    Hough_minRadius_slider->setStyleSheet("background-color:black;color:white");
    Hough_circles_thickness->setStyleSheet("background-color:black;color:white");
    Hough_maxRadius_SpinBox->setStyleSheet("background-color:black;color:white");
    Hough_minRadius_SpinBox->setStyleSheet("background-color:black;color:white");
    Hough_Radius_draw_in_src->setStyleSheet("background-color:black;color:white");
    Hough_circles_point_slider->setStyleSheet("background-color:black;color:white");
    Hough_circles_point_SpinBox->setStyleSheet("background-color:black;color:white");
    Hough_circles_canny_slider_h->setStyleSheet("background-color:black;color:white");
    Hough_circles_yuanxin_slider->setStyleSheet("background-color:black;color:white");
    Hough_circles_canny_SpinBox_h->setStyleSheet("background-color:black;color:white");
    Hough_circles_yuanxin_SpinBox->setStyleSheet("background-color:black;color:white");

    connect(Hough_circles_yuanxin_slider, &QAbstractSlider::valueChanged, Hough_circles_yuanxin_SpinBox, &QSpinBox::setValue);
    connect(Hough_circles_yuanxin_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Hough_circles_yuanxin_slider, &QAbstractSlider::setValue);

    connect(Hough_circles_yuanxin_slider, &QAbstractSlider::valueChanged, fun);
    connect(Hough_circles_yuanxin_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(Hough_circles_canny_slider_h, &QAbstractSlider::valueChanged, Hough_circles_canny_SpinBox_h, &QSpinBox::setValue);
    connect(Hough_circles_canny_SpinBox_h, QOverload<int>::of(&QSpinBox::valueChanged), Hough_circles_canny_slider_h, &QAbstractSlider::setValue);

    connect(Hough_circles_canny_slider_h, &QAbstractSlider::valueChanged, fun);
    connect(Hough_circles_canny_SpinBox_h, QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(Hough_circles_point_slider, &QAbstractSlider::valueChanged, Hough_circles_point_SpinBox, &QSpinBox::setValue);
    connect(Hough_circles_point_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Hough_circles_point_slider, &QAbstractSlider::setValue);

    connect(Hough_circles_point_slider, &QAbstractSlider::valueChanged, fun);
    connect(Hough_circles_point_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(Hough_minRadius_slider, &QAbstractSlider::valueChanged, Hough_minRadius_SpinBox, &QSpinBox::setValue);
    connect(Hough_minRadius_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Hough_minRadius_slider, &QAbstractSlider::setValue);

    connect(Hough_minRadius_slider, &QAbstractSlider::valueChanged, fun);
    connect(Hough_minRadius_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(Hough_maxRadius_slider, &QAbstractSlider::valueChanged, Hough_maxRadius_SpinBox, &QSpinBox::setValue);
    connect(Hough_maxRadius_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Hough_maxRadius_slider, &QAbstractSlider::setValue);

    connect(Hough_maxRadius_slider, &QAbstractSlider::valueChanged, fun);
    connect(Hough_maxRadius_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(Hough_Radius_draw_in_src, &QCheckBox::clicked, fun);
    connect(Hough_circles_thickness, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    fun();
}

//角点检测
void MainWindow::on_action_jiaodianjiance_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    Features_pixel_maxCorners_SpinBox = new QSpinBox(center_widget_in_dock);
    Features_pixel_maxCorners_SpinBox->setMinimum(50);
    Features_pixel_maxCorners_SpinBox->setMaximum(5000);
    Features_pixel_maxCorners_SpinBox->setValue(100);
    Features_pixel_maxCorners_SpinBox->setStyleSheet("background-color:black;color:white");

    Features_pixel_maxCorners_slider = new QSlider(center_widget_in_dock);
    Features_pixel_maxCorners_slider->setOrientation(Qt::Horizontal);
    Features_pixel_maxCorners_slider->setMinimum(50);
    Features_pixel_maxCorners_slider->setMaximum(5000);
    Features_pixel_maxCorners_slider->setSingleStep(50);
    Features_pixel_maxCorners_slider->setTickInterval(100);
    Features_pixel_maxCorners_slider->setTickPosition(QSlider::TicksAbove);
    Features_pixel_maxCorners_slider->setValue(100);
    Features_pixel_maxCorners_slider->setStyleSheet("background-color:black;color:white");

    Features_pixel_qualityLevel_SpinBox = new QSpinBox(center_widget_in_dock);
    Features_pixel_qualityLevel_SpinBox->setMinimum(1);
    Features_pixel_qualityLevel_SpinBox->setMaximum(99);
    Features_pixel_qualityLevel_SpinBox->setValue(1);
    Features_pixel_qualityLevel_SpinBox->setPrefix("0.0");//前缀
    Features_pixel_qualityLevel_SpinBox->setStyleSheet("background-color:black;color:white");

    Features_pixel_qualityLevel_slider = new QSlider(center_widget_in_dock);
    Features_pixel_qualityLevel_slider->setOrientation(Qt::Horizontal);
    Features_pixel_qualityLevel_slider->setMinimum(1);
    Features_pixel_qualityLevel_slider->setMaximum(99);
    Features_pixel_qualityLevel_slider->setSingleStep(10);
    Features_pixel_qualityLevel_slider->setTickInterval(10);
    Features_pixel_qualityLevel_slider->setTickPosition(QSlider::TicksAbove);
    Features_pixel_qualityLevel_slider->setValue(1);
    Features_pixel_qualityLevel_slider->setStyleSheet("background-color:black;color:white");

    Features_pixel_minDistance_SpinBox = new QSpinBox(center_widget_in_dock);
    Features_pixel_minDistance_SpinBox->setMinimum(3);
    Features_pixel_minDistance_SpinBox->setMaximum(20);
    Features_pixel_minDistance_SpinBox->setValue(3);//
    Features_pixel_minDistance_SpinBox->setStyleSheet("background-color:black;color:white");

    Features_pixel_minDistance_slider = new QSlider(center_widget_in_dock);
    Features_pixel_minDistance_slider->setOrientation(Qt::Horizontal);
    Features_pixel_minDistance_slider->setMinimum(3);
    Features_pixel_minDistance_slider->setMaximum(20);
    Features_pixel_minDistance_slider->setSingleStep(1);
    Features_pixel_minDistance_slider->setTickInterval(1);
    Features_pixel_minDistance_slider->setTickPosition(QSlider::TicksAbove);
    Features_pixel_minDistance_slider->setValue(3);
    Features_pixel_minDistance_slider->setStyleSheet("background-color:black;color:white");

    Features_pixel_blockSize_SpinBox = new QSpinBox(center_widget_in_dock);
    Features_pixel_blockSize_SpinBox->setMinimum(3);
    Features_pixel_blockSize_SpinBox->setMaximum(15);
    Features_pixel_blockSize_SpinBox->setValue(3);
    Features_pixel_blockSize_SpinBox->setStyleSheet("background-color:black;color:white");

    Features_pixel_blockSize_slider = new QSlider(center_widget_in_dock);
    Features_pixel_blockSize_slider->setOrientation(Qt::Horizontal);
    Features_pixel_blockSize_slider->setMinimum(3);
    Features_pixel_blockSize_slider->setMaximum(15);
    Features_pixel_blockSize_slider->setSingleStep(1);
    Features_pixel_blockSize_slider->setTickInterval(1);
    Features_pixel_blockSize_slider->setTickPosition(QSlider::TicksAbove);
    Features_pixel_blockSize_slider->setValue(3);
    Features_pixel_blockSize_slider->setStyleSheet("background-color:black;color:white");

    Features_pixel_draw_color = new QCheckBox(center_widget_in_dock);
    Features_pixel_draw_color->setText(QString("单色/多色"));
    Features_pixel_draw_color->setStyleSheet("background-color:black;color:white");

    Features_pixel_line_cuxi_SpinBox = new QSpinBox(center_widget_in_dock);
    Features_pixel_line_cuxi_SpinBox->setMinimum(-1);
    Features_pixel_line_cuxi_SpinBox->setMaximum(8);
    Features_pixel_line_cuxi_SpinBox->setValue(-1);
    Features_pixel_line_cuxi_SpinBox->setStyleSheet("background-color:black;color:white");

    Features_pixel_line_banjing_SpinBox = new QSpinBox(center_widget_in_dock);
    Features_pixel_line_banjing_SpinBox->setMinimum(3);
    Features_pixel_line_banjing_SpinBox->setMaximum(20);
    Features_pixel_line_banjing_SpinBox->setValue(3);
    Features_pixel_line_banjing_SpinBox->setStyleSheet("background-color:black;color:white");

    Features_type = new QComboBox(center_widget_in_dock);
    Features_type->addItem(QString("像素级角点检测"),1);
    Features_type->addItem(QString("亚像素级角点检测"),2);
    Features_type->setView(new QListView);
    Features_type->setStyleSheet("background-color:black;color:white");

    connect(Features_pixel_maxCorners_slider, &QAbstractSlider::valueChanged, Features_pixel_maxCorners_SpinBox, &QSpinBox::setValue);
    connect(Features_pixel_maxCorners_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Features_pixel_maxCorners_slider, QAbstractSlider::setValue);

    connect(Features_pixel_maxCorners_slider, &QAbstractSlider::valueChanged, this, &MainWindow::Features_pixel);
    connect(Features_pixel_maxCorners_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::Features_pixel);

    connect(Features_pixel_qualityLevel_slider, &QAbstractSlider::valueChanged, Features_pixel_qualityLevel_SpinBox, &QSpinBox::setValue);
    connect(Features_pixel_qualityLevel_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Features_pixel_qualityLevel_slider, QAbstractSlider::setValue);

    connect(Features_pixel_qualityLevel_slider, &QAbstractSlider::valueChanged, this, &MainWindow::Features_pixel);
    connect(Features_pixel_qualityLevel_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::Features_pixel);

    connect(Features_pixel_minDistance_slider, &QAbstractSlider::valueChanged, Features_pixel_minDistance_SpinBox, &QSpinBox::setValue);
    connect(Features_pixel_minDistance_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Features_pixel_minDistance_slider, QAbstractSlider::setValue);

    connect(Features_pixel_minDistance_slider, &QAbstractSlider::valueChanged, this, &MainWindow::Features_pixel);
    connect(Features_pixel_minDistance_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::Features_pixel);

    connect(Features_pixel_blockSize_slider, &QAbstractSlider::valueChanged, Features_pixel_blockSize_SpinBox, &QSpinBox::setValue);
    connect(Features_pixel_blockSize_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), Features_pixel_blockSize_slider, QAbstractSlider::setValue);

    connect(Features_pixel_blockSize_slider, &QAbstractSlider::valueChanged, this, &MainWindow::Features_pixel);
    connect(Features_pixel_blockSize_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::Features_pixel);

    connect(Features_pixel_line_cuxi_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::Features_pixel);
    connect(Features_pixel_draw_color, &QCheckBox::clicked, this, &MainWindow::Features_pixel);
    connect(Features_pixel_line_banjing_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &MainWindow::Features_pixel);
    connect(Features_type, QOverload<int>::of(&QComboBox::currentIndexChanged),this, &MainWindow::Features_pixel);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("角点检测类型：",0,0);
    grildlayout->addWidget(Features_type,0,1);
    grildLayoutAddWidgetLable("能检测出的最大角点数目：",1,0);
    grildlayout->addWidget(Features_pixel_maxCorners_SpinBox,1,1);
    grildlayout->addWidget(Features_pixel_maxCorners_slider,1,2,1,8);
    grildLayoutAddWidgetLable("角点强度系数：",2,0);
    grildlayout->addWidget(Features_pixel_qualityLevel_SpinBox,2,1);
    grildlayout->addWidget(Features_pixel_qualityLevel_slider,2,2,1,8);
    grildLayoutAddWidgetLable("角点间的最小距离：",3,0);
    grildlayout->addWidget(Features_pixel_minDistance_SpinBox,3,1);
    grildlayout->addWidget(Features_pixel_minDistance_slider,3,2,1,8);
    grildLayoutAddWidgetLable("检测角点时参与计算的区域大小：",4,0);
    grildlayout->addWidget(Features_pixel_blockSize_SpinBox,4,1);
    grildlayout->addWidget(Features_pixel_blockSize_slider,4,2,1,8);
    grildlayout->addWidget(Features_pixel_draw_color,5,0);
    grildLayoutAddWidgetLable("绘制圆线的粗细：",5,1);
    grildlayout->addWidget(Features_pixel_line_cuxi_SpinBox,5,2);
    grildLayoutAddWidgetLable("绘制圆的半径：",5,3);
    grildlayout->addWidget(Features_pixel_line_banjing_SpinBox,5,4);
    grildlayout->setContentsMargins(20,20,20,20);

    center_widget_in_dock->setLayout(grildlayout);

    Features_pixel();
}

//角点检测处理
void MainWindow::Features_pixel()
{
    if(Features_pixel_qualityLevel_slider->value() >= 10)
    {
        Features_pixel_qualityLevel_SpinBox->setPrefix("0.");//前缀
    }
    else
    {
        Features_pixel_qualityLevel_SpinBox->setPrefix("0.0");//前缀
    }

    std::vector<cv::Point2f> corners;
    cv::Mat image_gray;

    cv::cvtColor(srcImage, image_gray, cv::COLOR_BGR2GRAY);

    cv::goodFeaturesToTrack(image_gray,
                            corners,
                            Features_pixel_maxCorners_SpinBox->text().toInt(),
                            Features_pixel_qualityLevel_SpinBox->text().toCaseFolded().toDouble(),
                            Features_pixel_minDistance_SpinBox->text().toDouble(),
                            cv::Mat(),
                            Features_pixel_blockSize_SpinBox->text().toInt());

    srcImage.copyTo(dstImage);

    cv::Scalar color_single = cv::Scalar(rand()&255,rand()&255,rand()&255);
    cv::Scalar color;
    int banjing = Features_pixel_line_banjing_SpinBox->text().toInt();
    int cuxi = Features_pixel_line_cuxi_SpinBox->text().toInt();
    bool b;
    if(Features_pixel_draw_color->isChecked())
    {
        b = true;
    }
    else
    {
        b = false;
    }

    if(Features_type->currentData(Qt::UserRole).toInt() == 1)//像素级角点检测
    {
        int size = corners.size();
        for (int i = 0; i < size; ++i)
        {
            if(b)
            {
                color = cv::Scalar(rand()&255,rand()&255,rand()&255);
            }
            else
            {
                color = color_single;
            }

            cv::circle(dstImage,
                       corners[i],
                       banjing,
                       color,
                       cuxi);

            //绘制圆心
            cv::circle(dstImage,
                       corners[i],
                       2,
                       cv::Scalar(255,0,0),
                       -1);
        }
        dstlabel_show(srcImage,dstImage);
    }
    else //亚像素级角点检测
    {
        cv::cornerSubPix(image_gray,
                         corners,
                         cv::Size(Features_pixel_blockSize_SpinBox->text().toInt(), Features_pixel_blockSize_SpinBox->text().toInt()),
                         cv::Size(-1, -1),
                         cv::TermCriteria(cv::TermCriteria::MAX_ITER | cv::TermCriteria::EPS, 40, 0.01));

        int size = corners.size();
        for (int i = 0; i < size; ++i)
        {
            if(b)
            {
                color = cv::Scalar(rand()&255,rand()&255,rand()&255);
            }
            else
            {
                color = color_single;
            }

            cv::circle(dstImage,
                       corners[i],
                       banjing,
                       color,
                       cuxi);

            //绘制圆心
            cv::circle(dstImage,
                       corners[i],
                       2,
                       cv::Scalar(255,0,0),
                       -1);
        }
        dstlabel_show(srcImage,dstImage);
    }
}

//K-均值聚类
void MainWindow::on_actionK_Means_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QSpinBox * kmeans_num_SpinBox = new QSpinBox(center_widget_in_dock);
    kmeans_num_SpinBox->setMinimum(2);
    kmeans_num_SpinBox->setMaximum(20);
    kmeans_num_SpinBox->setValue(3);//
    kmeans_num_SpinBox->setFixedHeight(36);

    QSlider * kmeans_num_slider = new QSlider(center_widget_in_dock);
    kmeans_num_slider->setOrientation(Qt::Horizontal);
    kmeans_num_slider->setMinimum(2);
    kmeans_num_slider->setMaximum(20);
    kmeans_num_slider->setSingleStep(1);
    kmeans_num_slider->setTickInterval(1);
    kmeans_num_slider->setTickPosition(QSlider::TicksAbove);
    kmeans_num_slider->setValue(3);//
    kmeans_num_slider->setFixedHeight(36);

    QSpinBox * kmeans_time_SpinBox = new QSpinBox(center_widget_in_dock);
    kmeans_time_SpinBox->setMinimum(3);
    kmeans_time_SpinBox->setMaximum(20);
    kmeans_time_SpinBox->setValue(1);//
    kmeans_time_SpinBox->setFixedHeight(36);

    QSlider * kmeans_time_slider = new QSlider(center_widget_in_dock);
    kmeans_time_slider->setOrientation(Qt::Horizontal);
    kmeans_time_slider->setMinimum(3);
    kmeans_time_slider->setMaximum(20);
    kmeans_time_slider->setSingleStep(1);
    kmeans_time_slider->setTickInterval(1);
    kmeans_time_slider->setTickPosition(QSlider::TicksAbove);
    kmeans_time_slider->setValue(10);//
    kmeans_time_slider->setFixedHeight(36);

    kmeans_num_slider->setStyleSheet("background-color:black;color:white");
    kmeans_num_SpinBox->setStyleSheet("background-color:black;color:white");
    kmeans_time_slider->setStyleSheet("background-color:black;color:white");
    kmeans_time_SpinBox->setStyleSheet("background-color:black;color:white");

    connect(kmeans_time_slider, &QAbstractSlider::valueChanged, kmeans_time_SpinBox, &QSpinBox::setValue);
    connect(kmeans_time_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), kmeans_time_slider, &QAbstractSlider::setValue);
    connect(kmeans_num_slider, &QAbstractSlider::valueChanged, kmeans_num_SpinBox, &QSpinBox::setValue);
    connect(kmeans_num_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), kmeans_num_slider, &QAbstractSlider::setValue);

    QPushButton * kmeans_isok = new QPushButton("确定",center_widget_in_dock);
    kmeans_isok->setStyleSheet(QString("QPushButton{color: white;border-radius:5px;background:black;font-size:22px}"
                                       "QPushButton:hover{background: black;}"
                                       "QPushButton:pressed{background: #%3;}"));
                               //.arg(colorMain).arg(colorHover).arg(colorPressed));
    kmeans_isok->setFixedHeight(36);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("聚类分类数目：",0,0);
    grildlayout->addWidget(kmeans_num_SpinBox,0,1);
    grildlayout->addWidget(kmeans_num_slider,0,2);
    grildLayoutAddWidgetLable("提示：参数设置较大时会比较耗时",0,3);
    grildLayoutAddWidgetLable("K-Means算法执行次数：",1,0);
    grildlayout->addWidget(kmeans_time_SpinBox,1,1);
    grildlayout->addWidget(kmeans_time_slider,1,2);
    grildlayout->addWidget(kmeans_isok,1,3);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        int width = srcImage.cols;
        int height = srcImage.rows;

        //将图像像素转换为数据点
        cv::Mat points(width * height, srcImage.channels(), CV_32F);
        int index = 0;
        for (int i = 0; i < height; ++i)
        {
            for (int j = 0; j < width; ++j)
            {
                index = i * width + j;
                points.at<float>(index, 0) = srcImage.at<cv::Vec3b>(i, j)[0];
                points.at<float>(index, 1) = srcImage.at<cv::Vec3b>(i, j)[1];
                points.at<float>(index, 2) = srcImage.at<cv::Vec3b>(i, j)[2];
            }
        }

        //进行KMeans聚类
        int times = kmeans_num_SpinBox->value();

        cv::Mat bestLabels;
        cv::Mat centers(3, 3, CV_32F);
        //迭代参数
        cv::TermCriteria criteria = cv::TermCriteria(cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 1000, 0.001);
        cv::kmeans(points, times, bestLabels, criteria, kmeans_time_SpinBox->value(), cv::KMEANS_PP_CENTERS, centers);

        QList<cv::Scalar> colorList;
        for(int i = 0;i < times;++i)
        {
            colorList << CV_RGB( rand()&255, rand()&255, rand()&255 );
        }

        dstImage = cv::Mat::zeros(srcImage.size(), srcImage.type());
        for (int i = 0; i < height; ++i)
        {
            for (int j = 0; j < width; ++j)
            {
                index = i * width + j;
                int lable = bestLabels.at<int>(index, 0);
                dstImage.at<cv::Vec3b>(i, j)[0] = colorList[lable][0];//B
                dstImage.at<cv::Vec3b>(i, j)[1] = colorList[lable][1];//G
                dstImage.at<cv::Vec3b>(i, j)[2] = colorList[lable][2];//R
            }
        }
        dstlabel_show(srcImage,dstImage);
    };
    connect(kmeans_isok, &QPushButton::clicked,fun);
    fun();
}

//漫水填充（随机种子点）
void MainWindow::on_action_floodFill_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QSpinBox * floodfill_lowDifference_SpinBox = new QSpinBox(center_widget_in_dock);
    floodfill_lowDifference_SpinBox->setMinimum(0);
    floodfill_lowDifference_SpinBox->setMaximum(255);
    floodfill_lowDifference_SpinBox->setValue(0);//

    QSlider * floodfill_lowDifference_slider = new QSlider(center_widget_in_dock);
    floodfill_lowDifference_slider->setOrientation(Qt::Horizontal);
    floodfill_lowDifference_slider->setMinimum(0);
    floodfill_lowDifference_slider->setMaximum(255);
    floodfill_lowDifference_slider->setSingleStep(10);
    floodfill_lowDifference_slider->setTickInterval(10);
    floodfill_lowDifference_slider->setTickPosition(QSlider::TicksAbove);
    floodfill_lowDifference_slider->setValue(0);

    QSpinBox * floodfill_upDifference_SpinBox = new QSpinBox(center_widget_in_dock);
    floodfill_upDifference_SpinBox->setMinimum(0);
    floodfill_upDifference_SpinBox->setMaximum(255);
    floodfill_upDifference_SpinBox->setValue(0);

    QSlider * floodfill_upDifference_slider = new QSlider(center_widget_in_dock);
    floodfill_upDifference_slider->setOrientation(Qt::Horizontal);
    floodfill_upDifference_slider->setMinimum(0);
    floodfill_upDifference_slider->setMaximum(255);
    floodfill_upDifference_slider->setSingleStep(10);
    floodfill_upDifference_slider->setTickInterval(10);
    floodfill_upDifference_slider->setTickPosition(QSlider::TicksAbove);
    floodfill_upDifference_slider->setValue(0);

    QComboBox * floodfill_liantongxing = new QComboBox(center_widget_in_dock);
    floodfill_liantongxing->addItem(QString("4连通"),4);
    floodfill_liantongxing->addItem(QString("8连通"),8);
    floodfill_liantongxing->setView(new QListView());

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("像素连通性：",0,0);
    grildlayout->addWidget(floodfill_liantongxing,0,1);
    grildLayoutAddWidgetLable("当前点与种子点间颜色负差最大值：",1,0);
    grildlayout->addWidget(floodfill_lowDifference_SpinBox,1,1);
    grildlayout->addWidget(floodfill_lowDifference_slider,1,2,1,8);
    grildLayoutAddWidgetLable("当前点与种子点间颜色正差最大值：",2,0);
    grildlayout->addWidget(floodfill_upDifference_SpinBox,2,1);
    grildlayout->addWidget(floodfill_upDifference_slider,2,2,1,8);
    grildlayout->setContentsMargins(20,20,20,20);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        srcImage.copyTo(dstImage);

        int low_diff_int = floodfill_lowDifference_slider->value();
        int up_diff_int = floodfill_upDifference_slider->value();

        cv::Scalar low_diff = cv::Scalar(low_diff_int,low_diff_int,low_diff_int);
        cv::Scalar up_diff = cv::Scalar(up_diff_int,up_diff_int,up_diff_int);

        int flags = floodfill_liantongxing->currentData(Qt::UserRole).toInt() + (255<<8) + cv::FLOODFILL_FIXED_RANGE;

        cv::Rect ccomp;//定义重绘区域的最小边界矩形区域

        cv::floodFill(dstImage,
                  cv::Point(rand()&(dstImage.cols - 2) + 1,rand()&(dstImage.rows - 2) + 1), //随机种子点
                  cv::Scalar(rand()&255,rand()&255,rand()&255),
                  &ccomp,
                  low_diff,
                  up_diff,
                  flags);
        dstlabel_show(srcImage,dstImage);
    };

    floodfill_liantongxing->setStyleSheet("background-color:black;color:white");
    floodfill_upDifference_slider->setStyleSheet("background-color:black;color:white");
    floodfill_lowDifference_slider->setStyleSheet("background-color:black;color:white");
    floodfill_upDifference_SpinBox->setStyleSheet("background-color:black;color:white");
    floodfill_lowDifference_SpinBox->setStyleSheet("background-color:black;color:white");

    connect(floodfill_lowDifference_slider, &QAbstractSlider::valueChanged, floodfill_lowDifference_SpinBox, &QSpinBox::setValue);
    connect(floodfill_lowDifference_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), floodfill_lowDifference_slider, QAbstractSlider::setValue);

    connect(floodfill_lowDifference_slider, &QAbstractSlider::valueChanged, fun);
    connect(floodfill_lowDifference_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(floodfill_upDifference_slider, &QAbstractSlider::valueChanged, floodfill_upDifference_SpinBox, &QSpinBox::setValue);
    connect(floodfill_upDifference_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), floodfill_upDifference_slider, &QAbstractSlider::setValue);

    connect(floodfill_upDifference_slider, &QAbstractSlider::valueChanged, fun);
    connect(floodfill_upDifference_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    connect(floodfill_liantongxing, QOverload<int>::of(&QComboBox::currentIndexChanged),fun);
    fun();
}

//分水岭分割
void MainWindow::on_action_watershed_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }
    watershed_thresh2 = new QLabel(QString("高阈值：2~1000"),center_widget_in_dock);

    watershed_gausssize = new QComboBox(center_widget_in_dock);
    watershed_gausssize->setView(new QListView());
    for(int i = 3;i <= 20;i += 2)
    {
        watershed_gausssize->addItem(QString("核尺寸：%1×%1").arg(i),i);
    }
    connect(watershed_gausssize, SIGNAL(activated(const QString &)),this, SLOT(process_fenshuiling()));

    watershed_canny_l_SpinBox = new QSpinBox(center_widget_in_dock);
    watershed_canny_l_SpinBox->setMinimum(1);
    watershed_canny_l_SpinBox->setMaximum(254);
    watershed_canny_l_SpinBox->setValue(1);//

    watershed_canny_l_slider = new QSlider(center_widget_in_dock);
    watershed_canny_l_slider->setOrientation(Qt::Horizontal);
    watershed_canny_l_slider->setMinimum(1);
    watershed_canny_l_slider->setMaximum(254);
    watershed_canny_l_slider->setSingleStep(10);
    watershed_canny_l_slider->setTickInterval(10);
    watershed_canny_l_slider->setTickPosition(QSlider::TicksAbove);
    watershed_canny_l_slider->setValue(1);//

    connect(watershed_canny_l_slider, SIGNAL(valueChanged(int)), watershed_canny_l_SpinBox, SLOT(setValue(int)));
    connect(watershed_canny_l_SpinBox, SIGNAL(valueChanged(int)), watershed_canny_l_slider, SLOT(setValue(int)));

    connect(watershed_canny_l_slider, SIGNAL(valueChanged(int)), this, SLOT(process_fenshuiling()));
    connect(watershed_canny_l_SpinBox, SIGNAL(valueChanged(int)), this, SLOT(process_fenshuiling()));

    watershed_canny_h_SpinBox = new QSpinBox(center_widget_in_dock);
    watershed_canny_h_SpinBox->setMinimum(2);
    watershed_canny_h_SpinBox->setMaximum(500);
    watershed_canny_h_SpinBox->setValue(2);//

    watershed_canny_h_slider = new QSlider(center_widget_in_dock);
    watershed_canny_h_slider->setOrientation(Qt::Horizontal);
    watershed_canny_h_slider->setMinimum(2);
    watershed_canny_h_slider->setMaximum(500);
    watershed_canny_h_slider->setSingleStep(20);
    watershed_canny_h_slider->setTickInterval(40);
    watershed_canny_h_slider->setTickPosition(QSlider::TicksAbove);
    watershed_canny_h_slider->setValue(2);//

    connect(watershed_canny_h_slider, SIGNAL(valueChanged(int)), watershed_canny_h_SpinBox, SLOT(setValue(int)));
    connect(watershed_canny_h_SpinBox, SIGNAL(valueChanged(int)), watershed_canny_h_slider, SLOT(setValue(int)));

    connect(watershed_canny_h_slider, SIGNAL(valueChanged(int)), this, SLOT(process_fenshuiling()));
    connect(watershed_canny_h_SpinBox, SIGNAL(valueChanged(int)), this, SLOT(process_fenshuiling()));

    watershed_ronghe_SpinBox = new QSpinBox(center_widget_in_dock);
    watershed_ronghe_SpinBox->setMinimum(1);
    watershed_ronghe_SpinBox->setMaximum(9);
    watershed_ronghe_SpinBox->setValue(5);//

    watershed_ronghe_slider = new QSlider(center_widget_in_dock);
    watershed_ronghe_slider->setOrientation(Qt::Horizontal);
    watershed_ronghe_slider->setMinimum(1);
    watershed_ronghe_slider->setMaximum(9);
    watershed_ronghe_slider->setSingleStep(1);
    watershed_ronghe_slider->setTickInterval(1);
    watershed_ronghe_slider->setTickPosition(QSlider::TicksAbove);
    watershed_ronghe_slider->setValue(5);//

    watershed_thresh1->setStyleSheet("background-color:black;color:white");
    watershed_thresh2->setStyleSheet("background-color:black;color:white");
    watershed_gausssize->setStyleSheet("background-color:black;color:white");
    watershed_ronghe_slider->setStyleSheet("background-color:black;color:white");
    watershed_canny_h_slider->setStyleSheet("background-color:black;color:white");
    watershed_canny_l_slider->setStyleSheet("background-color:black;color:white");
    watershed_ronghe_SpinBox->setStyleSheet("background-color:black;color:white");
    watershed_canny_h_SpinBox->setStyleSheet("background-color:black;color:white");
    watershed_canny_l_SpinBox->setStyleSheet("background-color:black;color:white");

    connect(watershed_ronghe_slider, SIGNAL(valueChanged(int)), watershed_ronghe_SpinBox, SLOT(setValue(int)));
    connect(watershed_ronghe_SpinBox, SIGNAL(valueChanged(int)), watershed_ronghe_slider, SLOT(setValue(int)));

    connect(watershed_ronghe_slider, SIGNAL(valueChanged(int)), this, SLOT(process_fenshuiling()));
    connect(watershed_ronghe_SpinBox, SIGNAL(valueChanged(int)), this, SLOT(process_fenshuiling()));

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("预处理，高斯滤波核尺寸：",0,0);
    grildlayout->addWidget(watershed_gausssize,0,1);
    grildlayout->addWidget(new QLabel("低阈值：1~254"),1,0);
    grildlayout->addWidget(watershed_canny_l_SpinBox,1,1);
    grildlayout->addWidget(watershed_canny_l_slider,1,2,1,8);
    grildlayout->addWidget(watershed_thresh2,2,0);
    grildlayout->addWidget(watershed_canny_h_SpinBox,2,1);
    grildlayout->addWidget(watershed_canny_h_slider,2,2,1,8);
    grildLayoutAddWidgetLable("图像融合度：",3,0);
    grildlayout->addWidget(watershed_ronghe_SpinBox,3,1);
    grildlayout->addWidget(watershed_ronghe_slider,3,2,1,8);
    grildlayout->setContentsMargins(20,20,20,20);

    center_widget_in_dock->setLayout(grildlayout);

    process_watershed();
}

//处理分水岭分割
void MainWindow::process_watershed()
{
    int lowthreshint = watershed_canny_l_SpinBox->text().toInt();
    int threshdif = lowthreshint + 1;

    watershed_thresh2->setText(QString("高阈值：%1~1000").arg(threshdif));

    watershed_canny_h_slider->setMinimum(threshdif);
    watershed_canny_h_SpinBox->setMinimum(threshdif);

    //灰度化，滤波，Canny边缘检测
    cv::Mat imageGray;
    cvtColor(srcImage,imageGray,cv::COLOR_RGB2GRAY);//灰度转换
    int gaussize = watershed_gausssize->currentData(Qt::UserRole).toInt();
    cv::GaussianBlur(imageGray,imageGray,cv::Size(gaussize,gaussize),0,0);//高斯滤波
    Canny(imageGray,imageGray,lowthreshint,watershed_canny_h_SpinBox->text().toInt());

    //查找轮廓
    std::vector<std::vector<cv::Point>> contours;
    std::vector<cv::Vec4i> hierarchy;
    findContours(imageGray,contours,hierarchy,cv::RETR_TREE,cv::CHAIN_APPROX_SIMPLE,cv::Point());
    cv::Mat marks(srcImage.size(),CV_32S);
    marks = cv::Scalar::all(0);
    for(int index = 0,compCount = 0; index >= 0; index = hierarchy[index][0], ++compCount)
    {
        //对marks进行标记，对不同区域的轮廓进行编号，相当于设置注水点，有多少轮廓，就有多少注水点
        drawContours(marks, contours, index, cv::Scalar::all(compCount+1), 1, 8, hierarchy);
    }

    //我们来看一下传入的矩阵marks里是什么东西
    cv::Mat marksShows;
    convertScaleAbs(marks,marksShows);
    watershed(srcImage,marks);

    //我们再来看一下分水岭算法之后的矩阵marks里是什么东西
    cv::Mat afterWatershed;
    convertScaleAbs(marks,afterWatershed);

    //对每一个区域进行颜色填充
    cv::Mat PerspectiveImage = cv::Mat::zeros(srcImage.size(),CV_8UC3);
    int index,value;

    for(int i = 0;i < marks.rows;++i)
    {
        for(int j = 0;j < marks.cols;++j)
        {
            index = marks.at<int>(i,j);
            if(index == -1)
            {
                PerspectiveImage.at<cv::Vec3b>(i,j) = cv::Vec3b(255,255,255);
            }
            else
            {
                value = index;
                value = value % 255;  //生成0~255的随机数
                cv::RNG rng;
                PerspectiveImage.at<cv::Vec3b>(i,j) = cv::Vec3b(rng.uniform(0,value),rng.uniform(0,value),rng.uniform(0,value));
            }
        }
    }

    //分割并填充颜色的结果跟原始图像融合
    cv::Mat wshed;
    float ronghe = watershed_ronghe_SpinBox->text().toFloat() / 10.0;
    cv::addWeighted(srcImage,ronghe,PerspectiveImage,1.0 - ronghe,0,wshed);
    wshed.copyTo(dstImage);
    dstlabel_show(srcImage,dstImage);
}

//GrabCut抠图
void MainWindow::on_actionGrabCut_triggered()
{
    if (srcImage.empty())
     {
         return;
     }
     clearLayout();

     if(!control_widget->isHidden())
     {
         control_widget->hide();
     }

     //ui->labelsrc->action = my_label::Type::cut;//src操作模式改成截取模式
     action = MainWindow::selection::GrabCut;//GrabCut抠图模式
     if(dstImage.empty())
     {
         srcImage.copyTo(dstImage);
         dstlabel_show(srcImage,dstImage);
     }
}

//处理发送过来的矩形
void MainWindow::process_cut_rect(QRect r)
{
    if(action == MainWindow::selection::GrabCut)//GrabCut抠图
    {
        float rx = float(ui->label_src1->width()) / float(srcImage.size().width);
        float ry = float(ui->label_src1->height()) / float(srcImage.size().height);
        float x = r.x() / rx;
        float y = r.y() / ry;
        float width = r.width() / rx;
        float height = r.height() / ry;

        cv::Rect cv_r = cv::Rect(int(x),int(y),int(width),int(height));
        cv::Mat result, bg, fg;
        result = cv::Mat::zeros(srcImage.size(), CV_8UC1);

        cv::grabCut(srcImage, result, cv_r, bg, fg, 10, cv::GC_INIT_WITH_RECT);
        compare(result, cv::GC_PR_FGD, result, cv::CMP_EQ);

        dstImage = cv::Mat(srcImage.size(), CV_8UC3, cv::Scalar(255, 255, 255));
        srcImage.copyTo(dstImage, result);
        dstlabel_show(srcImage,dstImage);
    }
}

//MeanShift 均值漂移分割

void MainWindow::on_actionMeanShift_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(!control_widget->isHidden())
    {
        control_widget->hide();
    }

    cv::Mat res;                //分割后图像
    int spatialRad = 50;        //空间窗口大小
    int colorRad = 50;          //色彩窗口大小
    int maxPyrLevel = 2;        //金字塔层数
    cv::pyrMeanShiftFiltering(srcImage, res, spatialRad, colorRad, maxPyrLevel); //色彩聚类平滑滤波
    res.copyTo(dstImage);
    dstlabel_show(srcImage,dstImage);
}

//连通域检测

void MainWindow::on_action_connectzone_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QSpinBox * connectzone_thresh_SpinBox = new QSpinBox(center_widget_in_dock);
    connectzone_thresh_SpinBox->setMinimum(1);
    connectzone_thresh_SpinBox->setMaximum(255);
    connectzone_thresh_SpinBox->setValue(100);//
    connectzone_thresh_SpinBox->setFixedHeight(36);

    QSlider * connectzone_thresh_slider = new QSlider(center_widget_in_dock);
    connectzone_thresh_slider->setOrientation(Qt::Horizontal);
    connectzone_thresh_slider->setMinimum(1);
    connectzone_thresh_slider->setMaximum(255);
    connectzone_thresh_slider->setSingleStep(10);
    connectzone_thresh_slider->setTickInterval(10);
    connectzone_thresh_slider->setTickPosition(QSlider::TicksAbove);
    connectzone_thresh_slider->setValue(100);

    QSpinBox * connectzone_mianji_SpinBox = new QSpinBox(center_widget_in_dock);
    connectzone_mianji_SpinBox->setMinimum(1);
    connectzone_mianji_SpinBox->setMaximum(100000);
    connectzone_mianji_SpinBox->setValue(100);//
    connectzone_mianji_SpinBox->setFixedHeight(36);

    QSlider * connectzone_mianji_slider = new QSlider(center_widget_in_dock);
    connectzone_mianji_slider->setOrientation(Qt::Horizontal);
    connectzone_mianji_slider->setMinimum(1);
    connectzone_mianji_slider->setMaximum(100000);
    connectzone_mianji_slider->setSingleStep(100);
    connectzone_mianji_slider->setTickInterval(1000);
    connectzone_mianji_slider->setTickPosition(QSlider::TicksAbove);
    connectzone_mianji_slider->setValue(100);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildLayoutAddWidgetLable("阈值：",0,0);
    grildlayout->addWidget(connectzone_thresh_SpinBox,0,1);
    grildlayout->addWidget(connectzone_thresh_slider,0,2);
    grildLayoutAddWidgetLable("按面积筛选：",1,0);
    grildlayout->addWidget(connectzone_mianji_SpinBox,1,1);
    grildlayout->addWidget(connectzone_mianji_slider,1,2);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignHCenter);
    center_widget_in_dock->setLayout(grildlayout);

    auto fun = [=]
    {
        cv::Mat img, img_edge, labels, centroids, stats;
        cv::cvtColor(srcImage, img, cv::COLOR_BGR2GRAY);
        cv::threshold(img, img_edge, connectzone_thresh_SpinBox->value(), 255, cv::THRESH_BINARY);

        //连通域计算
        int nccomps = cv::connectedComponentsWithStats(img_edge,
                                                       labels,//和原图一样大的标记图
                                                       stats,//nccomps×5的矩阵 表示每个连通区域的外接矩形和面积
                                                       centroids);//nccomps×2的矩阵 表示每个连通区域的质心

        std::vector<cv::Vec3b> colors(nccomps + 1);
        colors[0] = cv::Vec3b(0,0,0); //黑色背景像素
        int mianji = connectzone_mianji_SpinBox->value();
        for(int i = 1;i <= nccomps;++i)
        {
            colors[i] = cv::Vec3b(rand()%256, rand()%256, rand()%256);
            if(stats.at<int>(i-1, cv::CC_STAT_AREA) < mianji)
            {
                colors[i] = cv::Vec3b(0,0,0); //小区域涂黑
            }
        }
        dstImage = cv::Mat::zeros(img.size(), CV_8UC3);
        int label;
        for(int y = 0; y < dstImage.rows;++y)
        {
            for(int x = 0; x < dstImage.cols;++x)
            {
                label = labels.at<int>(y, x);
                dstImage.at<cv::Vec3b>(y, x) = colors[label];
            }
        }

        dstlabel_show(srcImage,dstImage);
    };
    connectzone_mianji_slider->setStyleSheet("background-color:black;color:white");
    connectzone_thresh_slider->setStyleSheet("background-color:black;color:white");
    connectzone_mianji_SpinBox->setStyleSheet("background-color:black;color:white");
    connectzone_thresh_SpinBox->setStyleSheet("background-color:black;color:white");
    control_widget->setStyleSheet("background-color:black;color:white");
    connect(connectzone_thresh_slider, &QAbstractSlider::valueChanged, connectzone_thresh_SpinBox, &QSpinBox::setValue);
    connect(connectzone_thresh_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), connectzone_thresh_slider, &QAbstractSlider::setValue);
    connect(connectzone_thresh_slider, &QAbstractSlider::valueChanged, fun);
    connect(connectzone_thresh_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);

    connect(connectzone_mianji_slider, &QAbstractSlider::valueChanged, connectzone_mianji_SpinBox, &QSpinBox::setValue);
    connect(connectzone_mianji_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), connectzone_mianji_slider, &QAbstractSlider::setValue);
    connect(connectzone_mianji_slider, &QAbstractSlider::valueChanged, fun);
    connect(connectzone_mianji_SpinBox, QOverload<int>::of(&QSpinBox::valueChanged), fun);
    fun();
}

//SURF特征点检测

void MainWindow::on_actionSURF_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(!control_widget->isHidden())
    {
        control_widget->hide();
    }

    cv::Ptr<cv::xfeatures2d::SURF> detector = cv::xfeatures2d::SURF::create(400);//创建一个surf类对象并初始化
    std::vector<cv::KeyPoint> keypoints;
    detector->detect(srcImage, keypoints, cv::Mat());//找出关键点

    // 绘制关键点
    cv::drawKeypoints(srcImage, keypoints, dstImage, cv::Scalar::all(-1), cv::DrawMatchesFlags::DEFAULT);
    dstlabel_show(srcImage,dstImage);
}

//SIFT特征点检测

void MainWindow::on_actionSIFT_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(!control_widget->isHidden())
    {
        control_widget->hide();
    }

    cv::Ptr<cv::SIFT> detector = cv::SIFT::create(400);
    std::vector<cv::KeyPoint> keypoints;
    detector->detect(srcImage, keypoints, cv::Mat());

    cv::drawKeypoints(srcImage, keypoints, dstImage, cv::Scalar::all(-1), cv::DrawMatchesFlags::DEFAULT);
    dstlabel_show(srcImage,dstImage);
}

//SURF特征匹配

void MainWindow::on_actionSURF_2_triggered()
{
    if (srcImage.empty())
    {
        return;
    }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }

    QLabel * show_select_img_path = new QLabel(center_widget_in_dock);
    show_select_img_path->setWordWrap(true);
    show_select_img_path->setFixedWidth(600);

    QPushButton * select_img = new QPushButton("选择要和源图匹配的图片",center_widget_in_dock);
    select_img->setStyleSheet(QString("QPushButton{color: #FFFFFF;border-radius:5px;background:#%1;font-size:22px}"
                                      "QPushButton:hover{background: #%2;}"
                                      "QPushButton:pressed{background: #%3;}"));
                                         //.arg(colorMain).arg(colorHover).arg(colorPressed));
    select_img->setFixedSize(300,36);

    connect(select_img,&QPushButton::clicked,[show_select_img_path,this]
    {
        QString surfFileName = QFileDialog::getOpenFileName(this,
                                                            QString("打开图片文件"),
                                                            openfilepath.isNull() ? "/" : openfilepath, //QDir::currentPath()
                                                            QString("Image File(*.bmp *.jpg *.jpeg *.png)"));

        if(surfFileName.isEmpty())
        {
            return;
        }

        show_select_img_path->setText(surfFileName);
        surf_img = cv::imread(QTextCodec::codecForName("gb18030")->fromUnicode(surfFileName).data());
    });

    surf_img.release();

    point_grou_type = new QComboBox(center_widget_in_dock);
    point_grou_type->addItem("圆形",0);
    point_grou_type->addItem("矩形",1);
    point_grou_type->addItem("有角度的外接矩形",2);
    point_grou_type->setView(new QListView);
    point_grou_type->setEnabled(false);

    surf_process_type = new QComboBox(center_widget_in_dock);
    surf_process_type->addItem("特征点匹配",1);
    surf_process_type->addItem("特征点集拟合",2);
    surf_process_type->setView(new QListView);
    connect(surf_process_type,static_cast<void (QComboBox:: *)(int)>(&QComboBox::activated),[this]
    {
        if(surf_process_type->currentIndex() == 1)
        {
            point_grou_type->setEnabled(true);
        }
        else
        {
            point_grou_type->setEnabled(false);
        }
    });

    QLabel * label = new QLabel("特征点集拟合形状：");

    QPushButton * surf_ok = new QPushButton("确定",center_widget_in_dock);
    surf_ok->setStyleSheet(QString("QPushButton{color: #FFFFFF;border-radius:5px;background:#%1;font-size:22px}"
                                   "QPushButton:hover{background: #%2;}"
                                   "QPushButton:pressed{background: #%3;}"));//.arg(colorMain).arg(colorHover).arg(colorPressed));
    surf_ok->setFixedSize(150,36);
    connect(surf_ok,&QPushButton::clicked,this,&MainWindow::surf_process);

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(select_img,0,0);
    grildlayout->addWidget(show_select_img_path,0,1);
    grildlayout->addWidget(surf_process_type,0,2);
    grildlayout->addWidget(label,0,3);
    grildlayout->addWidget(point_grou_type,0,4);
    grildlayout->addWidget(surf_ok,0,5);

    grildlayout->setContentsMargins(20,20,20,20);

    grildlayout->setAlignment(Qt::AlignLeft);
    center_widget_in_dock->setLayout(grildlayout);
}

//surf特征匹配
void MainWindow::surf_process()
{
    if(surf_img.empty())
    {
        return;
    }

    if(surf_process_type->currentIndex() == 0)
    {
        //SURF 特征检测与匹配
        cv::Ptr<cv::xfeatures2d::SURF> detector = cv::xfeatures2d::SURF::create(1000);
        cv::Ptr<cv::DescriptorExtractor> descriptor = cv::xfeatures2d::SURF::create();
        cv::Ptr<cv::DescriptorMatcher> matcher1 = cv::DescriptorMatcher::create("BruteForce");

        std::vector<cv::KeyPoint> keyPoint1, keyPoint2;
        cv::Mat descriptors1, descriptors2;
        std::vector<cv::DMatch> matches;

        cv::Mat src;
        cv::cvtColor(srcImage,src,cv::COLOR_BGR2RGB);
        cvtColor(src,src,cv::COLOR_RGB2GRAY);

        cv::Mat surf_img02;
        cvtColor(surf_img,surf_img02,cv::COLOR_RGB2GRAY);

        // 检测特征点
        detector->detect(src, keyPoint1);
        detector->detect(surf_img02, keyPoint2);
        // 提取特征点描述子
        descriptor->compute(src, keyPoint1, descriptors1);
        descriptor->compute(surf_img02, keyPoint2, descriptors2);
        // 匹配图像中的描述子
        matcher1->match(descriptors1, descriptors2, matches);

        //提取强特征点
        double minMatch=1;
        double maxMatch=0;
        for(int i = 0;i < matches.size();++i)
        {
            //匹配值最大最小值获取
            minMatch = minMatch > matches[i].distance ? matches[i].distance:minMatch;
            maxMatch = maxMatch < matches[i].distance ? matches[i].distance:maxMatch;
        }

        //获取排在前边的几个最优匹配结果
        std::vector<cv::DMatch> goodMatchePoints;
        for(int i = 0;i < matches.size();++i)
        {
            if(matches[i].distance < minMatch + (maxMatch - minMatch)/2)
            {
                goodMatchePoints.push_back(matches[i]);
            }
        }

        if(goodMatchePoints.size() <= 0)
        {
            return;
        }

        //绘制最优匹配点
        drawMatches(src,keyPoint1,
                    surf_img02,
                    keyPoint2,
                    goodMatchePoints,
                    dstImage,
                    cv::Scalar::all(-1),
                    cv::Scalar::all(-1),
                    std::vector<char>(),
                    cv::DrawMatchesFlags::DEFAULT);//NOT_DRAW_SINGLE_POINTS
        imshow("Mathch Points",dstImage);
        cv::cvtColor(dstImage,dstImage,cv::COLOR_BGR2RGB);
        dstlabel_show(srcImage,dstImage);
    }
    else
    {
        cv::Ptr<cv::xfeatures2d::SURF> surf = cv::xfeatures2d::SURF::create(1000);
        cv::BFMatcher matcher;
        cv::Mat c,d;
        std::vector<cv::KeyPoint>key1, key2;
        std::vector<cv::DMatch> matches;

        cv::Mat src;
        cv::cvtColor(srcImage,src,cv::COLOR_BGR2RGB);
        cvtColor(src,src,cv::COLOR_RGB2GRAY);

        cv::Mat surf_img02;
        cvtColor(surf_img,surf_img02,cv::COLOR_RGB2GRAY);

        surf->detectAndCompute(src, cv::Mat(), key1, c);
        surf->detectAndCompute(surf_img02, cv::Mat(), key2, d);

        matcher.match(c, d, matches);//匹配

        sort(matches.begin(), matches.end());  //筛选匹配点
        std::vector<cv::DMatch > good_matches;
        int ptsPairs = std::min(50, (int)(matches.size() * 0.15));
        for (int i = 0; i < ptsPairs; i++)
        {
            good_matches.push_back(matches[i]);
        }
        cv::Mat outimg;
        drawMatches(src,
                    key1,
                    surf_img02,
                    key2,
                    good_matches,
                    outimg,
                    cv::Scalar::all(-1),
                    cv::Scalar::all(-1),
                    std::vector<char>(),
                    cv::DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);  //绘制匹配点

        std::vector<cv::Point2f> src_point;
        std::vector<cv::Point2f> surf_img_point;

        for (size_t i = 0; i < good_matches.size(); i++)
        {
            src_point.push_back(key1[good_matches[i].queryIdx].pt);
            surf_img_point.push_back(key2[good_matches[i].trainIdx].pt);
        }

        if(src_point.size() < 4 || surf_img_point.size() < 4)
        {
            return;
        }

        srcImage.copyTo(dstImage);
        cv::Mat surf_img_copy;
        surf_img.copyTo(surf_img_copy);

        if(point_grou_type->currentIndex() == 0)
        {
            cv::Point2f center;
            float radius = 0;
            minEnclosingCircle(src_point, center, radius);
            circle(dstImage, center, cvRound(radius), cv::Scalar(0, 0, 255), 3, cv::LINE_AA);

            radius = 0;
            minEnclosingCircle(surf_img_point, center, radius);
            circle(surf_img_copy, center, cvRound(radius), cv::Scalar(0, 0, 255), 3, cv::LINE_AA);
        }
        else if(point_grou_type->currentIndex() == 1)
        {
            std::vector<cv::Point> v;
            approxPolyDP(cv::Mat(src_point), v, 3, true);//外接多边形
            cv::Rect rr = boundingRect(cv::Mat(v));//从外接多边形获取外接矩形
            rectangle(dstImage, rr.tl(), rr.br(), cv::Scalar(0,255,0), 3);

            v.clear();
            approxPolyDP(cv::Mat(surf_img_point), v, 3, true);//外接多边形
            rr = boundingRect(cv::Mat(v));
            rectangle(surf_img_copy, rr.tl(), rr.br(), cv::Scalar(0,255,0), 3);
        }
        else if(point_grou_type->currentIndex() == 2)
        {
            cv::RotatedRect rRect = minAreaRect(src_point);
            cv::Point2f rect_Points[4];
            rRect.points(rect_Points);
            for (int j = 0; j < 4; ++j)
            {
                line(dstImage, rect_Points[j], rect_Points[(j + 1) % 4], cv::Scalar(255,0,0), 3, cv::LINE_AA);
            }

            rRect = minAreaRect(surf_img_point);
            rRect.points(rect_Points);
            for (int j = 0; j < 4; ++j)
            {
                line(surf_img_copy, rect_Points[j], rect_Points[(j + 1) % 4], cv::Scalar(255,0,0), 3, cv::LINE_AA);
            }
        }
        dstlabel_show(src,dstImage);
        cv::cvtColor(dstImage,dstImage,cv::COLOR_BGR2RGB);
        imshow("srcImage",dstImage);
        imshow("Compute",surf_img_copy);
    }
}


// 图像拼接
void MainWindow::on_action_pinjie_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }
    QLabel * show_select_img_path = new QLabel(center_widget_in_dock);
    show_select_img_path->setWordWrap(true);
    show_select_img_path->setFixedWidth(300);
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::black);
    show_select_img_path->setPalette(palette);

    QPushButton * select_img = new QPushButton("选择要和源图拼接的图片",center_widget_in_dock);
    select_img->setStyleSheet(QString("QPushButton{color:white;border-radius:5px;background:black;font-size:22px}"
                                      "QPushButton:hover{background:black;}"
                                      "QPushButton:pressed{background:black;}"));
                              //.arg(colorMain).arg(colorHover).arg(colorPressed));
    select_img->setFixedSize(300,36);

    QComboBox *pinjie = new QComboBox(center_widget_in_dock);
    pinjie->addItem(QString("基于 SURF"));
    pinjie->addItem(QString("膨胀 ORB"));
    pinjie->setView(new QListView());
    pinjie->setStyleSheet("background:black;color:white");

    grildlayout = new QGridLayout(center_widget_in_dock);
    grildlayout->addWidget(select_img,0,0);
    grildlayout->addWidget(show_select_img_path,0,1);
    grildLayoutAddWidgetLable(" 拼接方式：",0,2);
    grildlayout->addWidget(pinjie,0,3);
    grildlayout->setContentsMargins(20,20,20,20);
    grildlayout->setAlignment(Qt::AlignLeft);
    center_widget_in_dock->setLayout(grildlayout);

    seamlessRoi.release();
    connect(select_img,&QPushButton::clicked,[show_select_img_path,this]
    {
        QString surfFileName = QFileDialog::getOpenFileName(this,
                                                            QString("打开图片文件"),
                                                            openfilepath.isNull() ? "/" : openfilepath, //QDir::currentPath()
                                                            QString("Image File(*.bmp *.jpg *.jpeg *.png)"));

        if(surfFileName.isEmpty())
        {

            return;
        }

        show_select_img_path->setText(surfFileName);

        cv::Mat srcImage2 = cv::imread(QTextCodec::codecForName("gb18030")->fromUnicode(surfFileName).data());

        //将BGR转换为RGB，方便操作习惯
        cv::cvtColor(srcImage2,srcImage2,cv::COLOR_BGR2RGB);
        img = QImage((const unsigned char*)(srcImage2.data),
                     srcImage2.cols,
                     srcImage2.rows,
                     srcImage2.cols*srcImage2.channels(),
                     QImage::Format_RGB888);
        ui->label_src2->clear();

        //显示图像到QLabel控件

        ui->label_src2->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src2)));
        dstlabel_show(srcImage,dstImage);

    });
}

//***********************************图像拼接***************************//

// 基于SURF 图像拼接
using namespace cv;
using namespace cv::xfeatures2d;

typedef struct
{
   Point2f left_top;
   Point2f left_bottom;
   Point2f right_top;
   Point2f right_bottom;

}FOUR_CORNERS_T;

FOUR_CORNERS_T corners;

//计算配准图的四个顶点坐标
void MainWindow::CalcCorners(const Mat& H, const Mat& src)
{
    double v2[] = { 0, 0, 1 };//左上角
    double v1[3];//变换后的坐标值
    Mat V2 = Mat(3, 1, CV_64FC1, v2);  //列向量
    Mat V1 = Mat(3, 1, CV_64FC1, v1);  //列向量

    V1 = H * V2;
    //左上角(0,0,1)
    std::cout << "V2: " << V2 << std::endl;
    std::cout << "V1: " << V1 << std::endl;
    corners.left_top.x = v1[0] / v1[2];
    corners.left_top.y = v1[1] / v1[2];

    //左下角(0,src.rows,1)
    v2[0] = 0;
    v2[1] = src.rows;
    v2[2] = 1;
    V2 = Mat(3, 1, CV_64FC1, v2);  //列向量
    V1 = Mat(3, 1, CV_64FC1, v1);  //列向量
    V1 = H * V2;
    corners.left_bottom.x = v1[0] / v1[2];
    corners.left_bottom.y = v1[1] / v1[2];

    //右上角(src.cols,0,1)
    v2[0] = src.cols;
    v2[1] = 0;
    v2[2] = 1;
    V2 = Mat(3, 1, CV_64FC1, v2);  //列向量
    V1 = Mat(3, 1, CV_64FC1, v1);  //列向量
    V1 = H * V2;
    corners.right_top.x = v1[0] / v1[2];
    corners.right_top.y = v1[1] / v1[2];

    //右下角(src.cols,src.rows,1)
    v2[0] = src.cols;
    v2[1] = src.rows;
    v2[2] = 1;
    V2 = Mat(3, 1, CV_64FC1, v2);  //列向量
    V1 = Mat(3, 1, CV_64FC1, v1);  //列向量
    V1 = H * V2;
    corners.right_bottom.x = v1[0] / v1[2];
    corners.right_bottom.y = v1[1] / v1[2];
}

//优化两图的连接处，使得拼接自然
void MainWindow::OptimizeSeam(Mat& img1, Mat& trans, Mat& dst)
{
    int start = MIN(corners.left_top.x, corners.left_bottom.x);//开始位置，即重叠区域的左边界

    double processWidth = img1.cols - start;//重叠区域的宽度
    int rows = dst.rows;
    int cols = img1.cols; //注意，是列数*通道数
    double alpha = 1;//img1中像素的权重
    for (int i = 0; i < rows; i++)
    {
        uchar* p = img1.ptr<uchar>(i);  //获取第i行的首地址
        uchar* t = trans.ptr<uchar>(i);
        uchar* d = dst.ptr<uchar>(i);
        for (int j = start; j < cols; j++)
        {
            //如果遇到图像trans中无像素的黑点，则完全拷贝img1中的数据
            if (t[j * 3] == 0 && t[j * 3 + 1] == 0 && t[j * 3 + 2] == 0)
            {
                alpha = 1;
            }
            else
            {
                //img1中像素的权重，与当前处理点距重叠区域左边界的距离成正比，实验证明，这种方法确实好
                alpha = (processWidth - (j - start)) / processWidth;
            }

            d[j * 3] = p[j * 3] * alpha + t[j * 3] * (1 - alpha);
            d[j * 3 + 1] = p[j * 3 + 1] * alpha + t[j * 3 + 1] * (1 - alpha);
            d[j * 3 + 2] = p[j * 3 + 2] * alpha + t[j * 3 + 2] * (1 - alpha);

        }
    }

}


void MainWindow::SURF_pinjie()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();

    if(control_widget->isHidden())
    {
        control_widget->show();
    }
    QLabel * show_select_img_path = new QLabel(center_widget_in_dock);
    show_select_img_path->setWordWrap(true);
    show_select_img_path->setFixedWidth(300);
    QPalette palette;
    palette.setColor(QPalette::WindowText, Qt::black);
    show_select_img_path->setPalette(palette);

    QPushButton * select_img = new QPushButton("选择要和源图拼接的图片",center_widget_in_dock);
    select_img->setStyleSheet(QString("QPushButton{color:white;border-radius:5px;background:black;font-size:22px}"
                                      "QPushButton:hover{background:black;}"
                                      "QPushButton:pressed{background:black;}"));
                              //.arg(colorMain).arg(colorHover).arg(colorPressed));
    select_img->setFixedSize(300,36);

    QString surfFileName = QFileDialog::getOpenFileName(this,
                                                        QString("打开图片文件"),
                                                        openfilepath.isNull() ? "/" : openfilepath, //QDir::currentPath()
                                                        QString("Image File(*.bmp *.jpg *.jpeg *.png)"));

    if(surfFileName.isEmpty())
    {

        return;
    }

    show_select_img_path->setText(surfFileName);

    cv::Mat srcImage2 = cv::imread(QTextCodec::codecForName("gb18030")->fromUnicode(surfFileName).data());

    //将BGR转换为RGB，方便操作习惯
    cv::cvtColor(srcImage2,srcImage2,cv::COLOR_BGR2RGB);
    img = QImage((const unsigned char*)(srcImage2.data),
                 srcImage2.cols,
                 srcImage2.rows,
                 srcImage2.cols*srcImage2.channels(),
                 QImage::Format_RGB888);
    ui->label_src2->clear();

    //显示图像到QLabel控件

    ui->label_src2->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src2)));


    //connect(select_img, &QPushButton::clicked, [show_select_img_path, this]);

    Mat left =  srcImage;
    Mat right =  srcImage2;

    //创建surf对象
    //create参数 海森矩阵阀值
    Ptr<SURF> surf;
    surf = SURF::create(800);

    //实例化一个暴力匹配器
    BFMatcher matcher;

    std::vector<KeyPoint>key1,key2;
    Mat c,d;

    //寻找特征点
    surf->detectAndCompute(left,Mat(),key2,d);
    surf->detectAndCompute(right,Mat(),key1,c);

    //进行特征点对比 ，保存下来
    std::vector<DMatch> matches;
    matcher.match(d,c,matches);

    //排序 从小到大
    sort(matches.begin(),matches.end());

    //保存距离最短（最优）的特征点对象
    std::vector<DMatch>good_matches;
    int ptrPoint = std::min(50,(int)(matches.size()*0.15));
    for (int i = 0;i < ptrPoint;i++)
    {
        good_matches.push_back(matches[i]);
    }
    //最短距离的特征点连线
    Mat outimg;
    drawMatches(left,key2,right,key1,good_matches,outimg,
                Scalar::all(-1),Scalar::all(-1),
                std::vector<char>(),DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);



    //特征点配准
    std::vector<Point2f>imagepoint1,imagepoint2;

    for (int i=0;i<good_matches.size();i++)
    {
         imagepoint1.push_back(key1[good_matches[i].trainIdx].pt);
         imagepoint2.push_back(key2[good_matches[i].queryIdx].pt);
    }

    //透视转换
    Mat homo = findHomography(imagepoint1,imagepoint2,RANSAC);



    CalcCorners(homo,right);

    Mat imageTransForm;
    warpPerspective(right,imageTransForm,homo,
                    Size(MAX(corners.right_top.x,corners.right_bottom.x),left.rows));


    imageTransForm.copyTo(dstImage(Rect(0,0,imageTransForm.cols,imageTransForm.rows)));
    left.copyTo(dstImage(Rect(0,0,left.cols,left.rows)));

    //最终图片优化去除黑边
    OptimizeSeam(left,imageTransForm,dstImage);


    dstlabel_show(srcImage,dstImage);

}


// 基于ORB 图像拼接
void MainWindow::ORB_pinjie()
{
}

//工具栏基于 SURF 图像拼接
void MainWindow::on_action_SURF_pinjie_triggered()
{
    if (srcImage.empty())
        {
            return;
        }
    clearLayout();


    QString surfFileName = QFileDialog::getOpenFileName(this,
                                                        QString("打开图片文件"),
                                                        openfilepath.isNull() ? "/" : openfilepath, //QDir::currentPath()
                                                        QString("Image File(*.bmp *.jpg *.jpeg *.png)"));

    if(surfFileName.isEmpty())
    {

        return;
    }


    cv::Mat srcImage2 = cv::imread(QTextCodec::codecForName("gb18030")->fromUnicode(surfFileName).data());

    //将BGR转换为RGB，方便操作习惯
    cv::cvtColor(srcImage2,srcImage2,cv::COLOR_BGR2RGB);
    img = QImage((const unsigned char*)(srcImage2.data),
                 srcImage2.cols,
                 srcImage2.rows,
                 srcImage2.cols*srcImage2.channels(),
                 QImage::Format_RGB888);
    ui->label_src2->clear();

    //显示图像到QLabel控件

    ui->label_src2->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src2)));

    {
        QString surfFileName = QFileDialog::getOpenFileName(this,
                                                            QString("打开图片文件"),
                                                            openfilepath.isNull() ? "/" : openfilepath, //QDir::currentPath()
                                                            QString("Image File(*.bmp *.jpg *.jpeg *.png)"));

        if(surfFileName.isEmpty())
        {

            return;
        }


        cv::Mat srcImage2 = cv::imread(QTextCodec::codecForName("gb18030")->fromUnicode(surfFileName).data());

        //将BGR转换为RGB，方便操作习惯
        cv::cvtColor(srcImage2,srcImage2,cv::COLOR_BGR2RGB);
        img = QImage((const unsigned char*)(srcImage2.data),
                     srcImage2.cols,
                     srcImage2.rows,
                     srcImage2.cols*srcImage2.channels(),
                     QImage::Format_RGB888);
        ui->label_src2->clear();

        //显示图像到QLabel控件

        ui->label_src2->setPixmap(QPixmap::fromImage(ImageSetSize(img, ui->label_src2)));

    };

    Mat left =  srcImage;
    Mat right =  srcImage2;

    //创建surf对象
    //create参数 海森矩阵阀值
    Ptr<SURF> surf;
    surf = SURF::create(800);

    //实例化一个暴力匹配器
    BFMatcher matcher;

    std::vector<KeyPoint>key1,key2;
    Mat c,d;

    //寻找特征点
    surf->detectAndCompute(left,Mat(),key2,d);
    surf->detectAndCompute(right,Mat(),key1,c);

    //进行特征点对比 ，保存下来
    std::vector<DMatch> matches;
    matcher.match(d,c,matches);

    //排序 从小到大
    sort(matches.begin(),matches.end());

    //保存距离最短（最优）的特征点对象
    std::vector<DMatch>good_matches;
    int ptrPoint = std::min(50,(int)(matches.size()*0.15));
    for (int i = 0;i < ptrPoint;i++)
    {
        good_matches.push_back(matches[i]);
    }
    //最短距离的特征点连线
    Mat outimg;
    drawMatches(left,key2,right,key1,good_matches,outimg,
                Scalar::all(-1),Scalar::all(-1),
                std::vector<char>(),DrawMatchesFlags::NOT_DRAW_SINGLE_POINTS);



    //特征点配准
    std::vector<Point2f>imagepoint1,imagepoint2;

    for (int i=0;i<good_matches.size();i++)
    {
         imagepoint1.push_back(key1[good_matches[i].trainIdx].pt);
         imagepoint2.push_back(key2[good_matches[i].queryIdx].pt);
    }

    //透视转换
    Mat homo = findHomography(imagepoint1,imagepoint2,RANSAC);



    CalcCorners(homo,right);

    Mat imageTransForm;
    warpPerspective(right,imageTransForm,homo,
                    Size(MAX(corners.right_top.x,corners.right_bottom.x),left.rows));


    imageTransForm.copyTo(dstImage(Rect(0,0,imageTransForm.cols,imageTransForm.rows)));
    left.copyTo(dstImage(Rect(0,0,left.cols,left.rows)));

    //最终图片优化去除黑边
    OptimizeSeam(left,imageTransForm,dstImage);


    dstlabel_show(srcImage,dstImage);

}
//工具栏基于 ORB 图像拼接
void MainWindow::on_action_ORB_pinjie_triggered()
{

}

//软件说明（得搞个弹窗）
void MainWindow::on_action_about_triggered()
{
    //show_info("基于Qt5.14.2 & OpenCV4.4.0")
}

//帮助
void MainWindow::on_action_help_triggered()
{
    QDesktopServices::openUrl(QUrl(QLatin1String("https://www.qt.io/zh-cn/")));
}


