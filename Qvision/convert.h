﻿#ifndef CONVERT_H
#define CONVERT_H

/****************************************************************************
** file: convert.h
** brief: cv::Mat 与 QImage 之间的转换函数。在 Qt 程序中使用 OpenCV 时有用。
****************************************************************************/

#include <QtCore/QDebug>
#include <QtGui/QImage>
#include "opencv2/core/core.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "opencv2/opencv.hpp"
/**
 * @brief 将 OpenCV 的 cv::Mat 类型图像转换为 QImage 类型
 * @param mat 待转换的图像，支持 CV_8UC1、CV_8UC3、CV_8UC4 三种OpenCV 的数据类型
 * @param clone true 表示与 Mat 不共享内存，更改生成的 mat 不会影响原始图像，false 则会与 mat 共享内存
 * @param rb_swap 只针对 CV_8UC3 格式，如果 true 则会调换 R 与 B RGB->BGR，如果共享内存的话原始图像也会发生变化
 * @return 转换后的 QImage 图像
*/
QImage cvMat2QImage(const cv::Mat& mat, bool clone = true, bool rb_swap = true);

/**
 * @brief 将 QImage 的类型图像转换为 cv::Mat 类型
 * @param image 待转换的图像，支持 Format_Indexed8/Format_Grayscale、24 位彩色、32 位彩色格式，
 * @param clone true 表示与 QImage 不共享内存，更改生成的 mat 不会影响原始图像，false 则会与 QImage 共享内存
 * @param rg_swap 只针对 RGB888 格式，如果 true 则会调换 R 与 B RGB->BGR，如果共享内存的话原始图像也会发生变化
 * @return 转换后的 cv::Mat 图像
*/
cv::Mat QImage2cvMat(QImage &image, bool clone = false, bool rb_swap = true);


#endif // CONVERT_H
