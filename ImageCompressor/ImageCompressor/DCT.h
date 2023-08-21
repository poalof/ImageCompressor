/*
	���룺Mat

	1��Mat�ֳ�8x8�飬���㲹��
	2����ÿһ�飬����dct����õ�8x8��ϵ��������
	3�������п�ƴ����

	�����DCT/iDCTϵ��Mat
*/
#pragma once
#include "opencv2/opencv.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "iostream"
#include "fstream"
#include "regex"
#include "string.h"
#include "time.h"
#include "Windows.h"
#include "math.h"
#include "stdio.h"

using namespace cv;
using namespace std;

class DCT {
public:

	DCT()
	{
		DCTMat = Mat::zeros(8, 8, CV_64FC1);
		iDCTMat = Mat::zeros(8, 8, CV_64FC1);
		mask = Mat::zeros(8, 8, CV_64FC1);

		SetDCTMat();
		SetiDCTMat();
		SetMask();
	}

	void SetDCTMat() // ����dct����
	{
		for (int i = 0; i < 8; i++)
			for (int j = 0; j < 8; j++) 
			{
				double alpha = (i == 0) ? sqrt(1.0f / 8) : sqrt(2.0f / 8);
				DCTMat.at<double>(i, j) = alpha * cos((j + 0.5f) * CV_PI * i / 8);
			}

	}

	void SetiDCTMat() // ��dct����
	{
		for (int j = 0; j < 8; ++j)
			for (int i = 0; i < 8; ++i)
				iDCTMat.at<double>(i, j) = DCTMat.at<double>(j, i);
	}

	void SetMask() // ��������
	{
		mask = (Mat_<double>(8, 8)
		 << 1, 1, 1, 1, 1, 0, 0, 0,
			1, 1, 1, 1, 1, 0, 0, 0,
			1, 1, 1, 1, 0, 0, 0, 0,
			1, 1, 1, 0, 0, 0, 0, 0,
			1, 1, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0,
			0, 0, 0, 0, 0, 0, 0, 0);
	}

	Mat DCT8x8(Mat image); // DCT�任

	Mat iDCT8x8(Mat image); // ��任

private:
	Mat DCTMat;
	Mat iDCTMat;
	Mat mask; // 8x8 ������ʵ����ֱ�Ӱ�ϵ��ȥ���ˣ�
};