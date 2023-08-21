/***
	����
	input����Ҫ�������������uchar��
	output�������������
	
	����
	input��ֵƵ�ʱ�&��������
	output��uchar������
***/

#pragma once
#include <iostream>
#include <cstring>
#include <vector>
#include <queue>
#include <unordered_map>

using namespace std;
// ��Ϊɶ����ô��vector

class HuffmanCode {
public:
	// �������ڵ�
	struct HuffmanNode {
		HuffmanNode* left;
		HuffmanNode* right;
		uint32_t w;        // ��ǰ�ڵ�Ȩ��
		int val; // ����ֵ��һ���ֽ���Ϊ��������uchar��[0,255]
		HuffmanNode(int val, uint32_t w) : 
			val(val), w(w), left(nullptr), right(nullptr) {}
	};

	HuffmanCode(){ treeRoot = nullptr; }

	void BuildTree(); // ����Ȩ�ر���

	void SetWeightTable(const vector<int>& data); // Ȩ�ر�

	void SetCodeTable(); // ���ɱ����dfs���������� �õ�Ҷ�ڵ�val��path�ı���

	void SearchCode(HuffmanNode* root, vector<char> code); // dfs

	vector<char> CharToBit(vector<char>& ); // ���յ�bit��

	vector<char> BitToChar(const vector<char>& , uint32_t );

	vector<char> Encode(const vector<int>& data); // �������

	vector<int> Decode(const vector<char>& bitSeq); // ����

	vector<char> SerializeMap();

	void DeserializeMap(vector<char> bitSeq, int size);

private:
	unordered_map<int, uint32_t> valToWeight; // val��Ӧ��Ƶ��&Ȩ��
	unordered_map<int, vector<char> > valToCode; // val��Ӧ���룬����ı�����'0''1'��ɣ����������������ļ���bs����uchar -> bit
	HuffmanNode* treeRoot; // ���ڵ�
	struct cmp {
		bool operator()(HuffmanNode* a, HuffmanNode* b) {
			if (a->w == b->w)
				return a->val > b->val;
			return a->w > b->w;
		}
	};
	priority_queue <HuffmanNode*, vector<HuffmanNode*>, cmp> remainNode; // ���ڽ����Ľڵ����

};


