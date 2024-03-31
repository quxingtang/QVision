#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QLabel>
#include <QDockWidget>
#include <QListWidget>
#include <QTextEdit>
#include <QFileDialog>
#include <QFontDialog>
#include <QColorDialog>
#include <QFile>
#include <QTextCodec>
#include <QMessageBox>
#include <QTextStream>
#include <QDesktopServices>
#include <QComboBox>
#include <QSpinBox>
#include <QGridLayout>
#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    cv::Mat srcImage;
    cv::Mat dstImage;
    QString src_path;
    QImage img;
    QImage img2;
    QString imgfilename;
    QString openfilepath;
    QString savefilepath;
    int save_num = 1;

    QLabel*    show_input_lunkuojiance_thresh;
    QSlider*   lunkuojiance_mianji_shaixuan_slider;
    QSpinBox*  lunkuojiance_mianji_shaixuan_SpinBox;
    QSlider*   lunkuojiance_slider_h;
    QSpinBox*  lunkuojiance_SpinBox_h;
    QSlider*   lunkuojiance_slider_l;
    QSpinBox*  lunkuojiance_SpinBox_l;
    QComboBox* lunkuojiance_color_QComboBox;
    QComboBox* lunkuojiance_RETR_QComboBox;
    QComboBox* lunkuojiance_APPROX_QComboBox;
    QComboBox* lunkuojiance_lunkuocuxi_QComboBox;
    QCheckBox* lunkuojiance_isdraw_ju;
    QCheckBox* lunkuojiance_draw_in_src;
    QCheckBox* lunkuojiance_draw_hull;
    QCheckBox* lunkuojiance_draw_poly;
    QCheckBox* lunkuojiance_draw_rectangle;
    QCheckBox* lunkuojiance_draw_circle;
    QCheckBox* lunkuojiance_draw_AreaRect;
    QCheckBox* lunkuojiance_draw_Ellipse;
    QComboBox* canny_ksize_lunkuojiance;
    QComboBox* L2gradient_lunkuojiance;

    QSpinBox*  Features_pixel_maxCorners_SpinBox;
    QSlider*   Features_pixel_maxCorners_slider;
    QSpinBox*  Features_pixel_minDistance_SpinBox;
    QSlider*   Features_pixel_minDistance_slider;
    QSpinBox*  Features_pixel_qualityLevel_SpinBox;
    QSlider*   Features_pixel_qualityLevel_slider;
    QSpinBox*  Features_pixel_blockSize_SpinBox;
    QSlider*   Features_pixel_blockSize_slider;
    QCheckBox* Features_pixel_draw_color;
    QSpinBox*  Features_pixel_line_cuxi_SpinBox;
    QSpinBox*  Features_pixel_line_banjing_SpinBox;
    QComboBox* Features_type;

    QGridLayout* grildlayout{nullptr};

    bool is_continuity_process{false};
    QDockWidget *control_widget;
    QWidget * center_widget_in_dock;

    int point_num;
    cv::Mat src_copy_for_fangshe;
    QList<cv::Point> point_list;

    QLabel *    watershed_thresh1;
    QLabel *    watershed_thresh2;
    QComboBox * watershed_gausssize;
    QSpinBox*   watershed_canny_l_SpinBox;
    QSlider*    watershed_canny_l_slider;
    QSpinBox*   watershed_canny_h_SpinBox;
    QSlider*    watershed_canny_h_slider;
    QSpinBox*   watershed_ronghe_SpinBox;
    QSlider*    watershed_ronghe_slider;

    cv::Mat     surf_img;
    QComboBox * surf_process_type;
    QComboBox * point_grou_type;

    //泊松融合
    cv::Mat seamlessRoi;

protected:
    QImage ImageSetSize(QImage  qimage,QLabel *qLabel);

    void clearLayout();

    void dstlabel_show(const cv::Mat & srcImage_temp, const cv::Mat & dstImage_temp);

    void dstlabel_indexed8_show(const cv::Mat &dstImage_temp);

    void grildLayoutAddWidgetLable(QString str,int x, int y);

    void lunkuojiance_process();

    void dropEvent(QDropEvent *event);

    void dragEnterEvent(QDragEnterEvent *event);

    void process_watershed();

    void process_cut_rect(QRect r);

    void Features_pixel();

    void surf_process();

    void ORB_pinjie();

    void SURF_pinjie();

    void CalcCorners(const cv::Mat &H, const cv::Mat &src);

private slots:
    void on_action_open_triggered();

    void on_action_save_triggered();

    void on_action_clear_triggered();

    void on_action_back_triggered();

    void on_action_exit_triggered();

    void on_action_select_scope_triggered();

    void on_action_bulr_triggered();


    void on_action_boxFilter_triggered();

    void on_action_GaussFilter_triggered();

    void on_action_midian_triggered();

    void on_action_bilateral_triggered();

    void on_action_equalizehist_triggered();

    void on_action_tiaojie_liangdu_duibidu_triggered();

    void on_action_base_triggered();

    void on_actionTHRESH_OTSU_triggered();

    void on_actionTHRESH_TRIANGLE_triggered();

    void on_action_adapt_triggered();

    void on_actionSobel_triggered();

    void on_actionCanny_triggered();

    void on_actionLaplacian_triggered();

    void on_actionScharr_triggered();

    void on_action_about_triggered();

    void on_action_lunkuojiance_triggered();

    void on_action_huofu_line_triggered();

    void on_action_huofu_yuan_triggered();

    void on_action_jiaodianjiance_triggered();

    void on_action_warpAffine_triggered();

    void on_action_logPolar_triggered();

    void on_actionK_Means_triggered();

    void on_action_kx_b_triggered();

    void on_action_floodFill_triggered();

    void on_action_toushibianhuan_triggered();

    void on_action_watershed_triggered();

    void on_action_hist_line_triggered();

    void on_action_hist_colu_triggered();

    void on_actionGrabCut_triggered();

    void on_actionMeanShift_triggered();

    void on_action_connectzone_triggered();

    void on_actionSURF_triggered();

    void on_actionSIFT_triggered();

    void on_actionSURF_2_triggered();

    void on_actionGuidedFilter_triggered();

    void on_actionPoissonFusion_triggered();

    void on_action_help_triggered();

    void on_action_pinjie_triggered();

    void on_action_SURF_pinjie_triggered();

    void on_action_ORB_pinjie_triggered();

    void OptimizeSeam(cv::Mat& img1, cv::Mat& trans, cv::Mat& dst);
private:
    Ui::MainWindow *ui;

    QMenuBar *menuBar;//菜单栏
    QToolBar *toolBar;//工具栏

    enum class selection
    {
        toushibianhuan,//透视变换
        GrabCut,//GrabCut抠图
        poissonFusion,//泊松融合
        nothing
    };
    selection action;


};
#endif // MAINWINDOW_H
