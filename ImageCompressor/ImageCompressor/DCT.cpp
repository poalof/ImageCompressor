#include "opencv2/opencv.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "iostream"
#include "math.h"

#include "DCT.h"

Mat DCT::DCT8x8(Mat image) // DCT�任�����ص�ͼ��padding����,int����
{
	// ��8����������
	int width = image.cols % 8== 0 ? image.cols: image.cols + 8 - image.cols % 8; // ��ȫ���ͼ����
	int height = image.rows % 8 == 0 ? image.rows: image.rows + 8 - image.rows % 8; // ��ȫ���ͼ��߶�
	Mat output = Mat::zeros(height, width, CV_64FC1); // �任���ͼ��
	Mat paddedImage; // ����8x8��ԭͼ
	copyMakeBorder(image, paddedImage, 0, height - image.rows, 0, width - image.cols, BORDER_CONSTANT, Scalar(0));
	paddedImage.convertTo(paddedImage, CV_64FC1);
	for (int y = 0; y < height; y += 8) 
	{
		for (int x = 0; x < width; x += 8) 
		{
			Mat block = paddedImage(Rect(x, y, 8, 8));
			Mat dctBlock = DCTMat * block * iDCTMat; // dct
			dctBlock = mask.mul(dctBlock); // ����
			dctBlock.copyTo(output(Rect(x, y, 8, 8)));
		}
	}

	// �ü�
	//output = output(Range(0, image.rows), Range(0, image.cols));
	output.convertTo(output, CV_32SC1);

	return output;
}

Mat DCT::iDCT8x8(Mat image)	// DCT��任�����ص�ͼ��padding���ģ�uchar����
{
	// ��8����������
	int width = image.cols % 8 == 0 ? image.cols : image.cols + 8 - image.cols % 8; // ��ȫ���ͼ����
	int height = image.rows % 8 == 0 ? image.rows : image.rows + 8 - image.rows % 8; // ��ȫ���ͼ��߶�
	Mat output = Mat::zeros(height, width, CV_64FC1); // ��任���ͼ��
	Mat paddedImage; // ����8x8��ԭͼ
	copyMakeBorder(image, paddedImage, 0, height - image.rows, 0, width - image.cols, BORDER_CONSTANT, Scalar(0));
	paddedImage.convertTo(paddedImage, CV_64FC1);
	for (int y = 0; y < height; y += 8)
	{
		for (int x = 0; x < width; x += 8)
		{
			Mat block = paddedImage(Rect(x, y, 8, 8));
			Mat dctBlock = iDCTMat * block * DCTMat; // idct
			dctBlock.copyTo(output(Rect(x, y, 8, 8)));
		}
	}

	//output = output(Range(0, image.rows), Range(0, image.cols));
	output.convertTo(output, CV_8UC1);

	return output;
}


