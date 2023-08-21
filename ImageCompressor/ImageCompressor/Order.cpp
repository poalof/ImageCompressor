// Zigzag���У���8x8��DCTϵ���Ӹ�Ƶ����Ƶ����

#include "opencv2/opencv.hpp"
#include "opencv2/core/core.hpp"
#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"
#include "iostream"
#include "math.h"
#include "Order.h"


vector<int> Order::ZigZag(Mat dct)
{
	vector<int> zigzagOrder(64);
	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 8; j++)
			zigzagOrder[zigzagTable[i * 8 + j]] = dct.at<int>(i, j);
	}

	return zigzagOrder;
}

Mat Order::iZigZag(vector<int> order)
{
	Mat reOrder(8, 8, CV_32SC1);
	for (int i = 0; i < 8; i++)
	{
		for (int j = 0; j < 8; j++)
			reOrder.at<int>(i,j) = order[zigzagTable[i*8+j]];
	}

	return reOrder;
}

// RLE ����
vector<int> Order::RLE_Encode(const vector<int>& data) 
{
	vector<int> encoded_data;
	int zero_count = 0;

	for (int i = 0; i < data.size(); i++) 
	{
		if (data[i] == 0)
			zero_count++;
		else 
		{
			encoded_data.push_back(zero_count);
			encoded_data.push_back(data[i]);
			zero_count = 0;
		}
	}

	// ���ĩβ������ĸ���
	// ������Ĵ�СΪ����
	encoded_data.push_back(zero_count);

	return encoded_data;
}

// RLE ����
vector<int> Order::RLE_Decode(const vector<int>& encoded_data) 
{
	vector<int> decoded_data;
	int zero_count = 0;

	for (int i = 0; i < encoded_data.size(); i++) 
	{
		if (i % 2 == 0)
			// ������ĸ���
			zero_count = encoded_data[i];
		else 
		{
			// ����ֵ
			int value = encoded_data[i];
			while (zero_count > 0) 
			{
				decoded_data.push_back(0);
				zero_count--;
			}
			decoded_data.push_back(value);
		}
	}

	// ĩβ��0
	while (zero_count-- > 0)
		decoded_data.push_back(0);

	return decoded_data;
}


