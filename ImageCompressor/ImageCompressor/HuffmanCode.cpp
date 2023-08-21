#include <iostream>
#include <cstring>
#include <vector>
#include <queue>
#include <unordered_map>
#include "HuffmanCode.h"

using namespace std;

void HuffmanCode::SetWeightTable(const vector<int>& data)
{
	for (int i = 0; i < data.size(); i++)
	{
		valToWeight[data[i]]++;
	}
}

void HuffmanCode::BuildTree()
{
	for (auto i : valToWeight)
	{
		remainNode.push(new HuffmanNode(i.first, i.second));
	}
	
	// ��������������ˣ��Ǻ�
	int cnt = 0x3f3f3f3f;
	while (remainNode.size() > 1)
	{
		HuffmanNode* l = remainNode.top();
		remainNode.pop();
		HuffmanNode* r = remainNode.top();
		remainNode.pop();
		HuffmanNode* newRoot = new HuffmanNode(cnt++, l->w + r->w); // ��Ҷ�ӽڵ㲻����val
		newRoot->left = l;
		newRoot->right = r;
		remainNode.push(newRoot);
	}

	treeRoot = remainNode.top(); // ���յ�����
}

// dfs
void HuffmanCode::SearchCode(HuffmanNode* root, vector<char> code)
{
	
	if (root->left == nullptr && root->right == nullptr)
	{
		valToCode[root->val] = code; // val -> code
		return;
	}

	code.push_back('0');
	SearchCode(root->left, code);

	code[code.size() - 1] = '1';
	SearchCode(root->right, code);
}

void HuffmanCode::SetCodeTable()
{
	vector<char> empty;

	if(treeRoot != nullptr)
		SearchCode(treeRoot, empty);
}

vector<char> HuffmanCode::CharToBit(vector<char>& charSeq) // ���յ��ֽ���
{
	// ��char��λ����λ����
	// ��char����Ϊ 0 0 0 0 1 0 1 1
	// �����char����ǣ����λ������λ��Ϊ0��...�����λ������λ��Ϊ1
	vector<char> bitSeq(1);
	int bit = 0;
	for (int i = 0; i < charSeq.size(); i++)
	{
		int code = charSeq[i] - '0';
		if (bit == 8)
		{
			bitSeq.push_back(0);
			bit = 0;
		}

		bitSeq[bitSeq.size() - 1] |= code << (7 - bit);
		bit++;
	}

	return bitSeq;
}

vector<char> HuffmanCode::SerializeMap()
{
	vector<uint32_t> data;
	uint32_t size = 0;
	for (auto i : valToWeight)
	{
		data.push_back(i.first);
		data.push_back(i.second);
		size++;
	}
	data.insert(data.begin(), size); // ͷ��Ϊ���С/����
	
	uint32_t *p = new uint32_t[data.size()];
	for (int i = 0; i < data.size(); i++)
		p[i] = data[i];

	char *p2 = reinterpret_cast<char*>(p);
	vector<char> result;
	for (int i = 0; i < data.size() * 4; i++)
		result.push_back(p2[i]);
	
	//for (int i = 0; i < result.size(); i++)
		//cout << (int)result[i] << " ";

	delete[] p;

	return result;
}

void HuffmanCode::DeserializeMap(vector<char> bitSeq, int size)
{
	uint32_t index = 4;
	for (int i = 0; i < size; i++)
	{
		char val[4];
		char weight[4];
		for (int j = 0; j < 4; j++)
			val[j] = bitSeq[index++];
		for (int j = 0; j < 4; j++)
			weight[j] = bitSeq[index++];
		int* pval = reinterpret_cast<int*>(val);
		int* pweight = reinterpret_cast<int*>(weight);
		valToWeight[*pval] = *pweight;

	}
}

vector<char> HuffmanCode::Encode(const vector<int>& data)
{
	// ��ԭ������֮�⣬�����������valToWeight�������ֽ������л�������ͷ��
	// headLength | head(hashmap) | bitLength | bitSequence
	vector<char> rawCode; // ���� char
	vector<char> result; // �������ص�bitstream
	uint32_t headLen = 0; // ���л��õ���ͷ������

	int bitSize = 0;
	SetWeightTable(data);
	// map
	result = SerializeMap();
	// ֻ��һ��val���ļ�: map 12 | bitsize 4 | bitseq
	if (valToWeight.size() == 1)
	{
		vector<char> res(data.size(), 0);
		res = CharToBit(res);
		int p = data.size();
		char* ptr = reinterpret_cast<char*>(&p);
		vector<char> bs(4);
		for (int i = 0; i < 4; i++)
			bs[i] = *(ptr+i);
		res.insert(res.begin(), bs.begin(), bs.end());
		res.insert(res.begin(), result.begin(), result.end());
		return res;
	}

	BuildTree();
	SetCodeTable();

	for (int i = 0; i < data.size(); i++)
	{
		vector<char> code = valToCode[data[i]]; // �����val
		for (int j = 0; j < code.size(); j++)
		{
			rawCode.push_back(code[j]);
			bitSize++;
		}
	}

	// bitsize
	char* ptr = reinterpret_cast<char*>(&bitSize);
	vector<char> tmp;
	for (int i = 0; i < 4; i++)
		tmp.push_back(ptr[i]);
	result.insert(result.end(), tmp.begin(), tmp.end());
	//cout << "bitsize: " << bitSize << endl;

	vector<char> bitData = CharToBit(rawCode);
	result.insert(result.end(), bitData.begin(), bitData.end());
	
	return result;
}

vector<char> HuffmanCode::BitToChar(const vector<char>& bitSeq, uint32_t bitCount)
{
	vector<char> charSeq(bitCount);
	int cnt = 0; // char vector�±�
	int bitIdx = 0; // bit vector�±�
	int bit = 0; // ��ǰchar��λ��

	while (cnt < bitCount)
	{
		if (bit == 8) // ��ǰchar������
		{
			bitIdx++;
			bit = 0;
		}
		charSeq[cnt] = ((bitSeq[bitIdx]&(1<<(7-bit))) >> (7-bit)) + '0';
		cnt++;
		bit++;
	}

	return charSeq;
}

vector<int> HuffmanCode::Decode(const vector<char>& bitSeq) // ����
{
	// ��char����ȡmap���õ�bitsize���ٽ���
	// ǰ4���ֽ�--mapԪ�ظ���
	char p[4];
	for (int i = 0; i < 4; i++)
		p[i] = bitSeq[i];
	uint32_t *headLen = reinterpret_cast<uint32_t*>(p); // map size
	//cout << "map size: " << *headLen << endl;

	// ��ʼ��
	treeRoot = nullptr;
	remainNode = priority_queue <HuffmanNode*, vector<HuffmanNode*>, cmp>();
	valToWeight.clear();
	valToCode.clear();
	
	// �õ�Ȩ�ر�
	DeserializeMap(bitSeq, *headLen);

	// ��ȡbitsize
	uint32_t dataStart = *headLen * 8+ 4;
	for (int i = 0; i < 4; i++)
		p[i] = bitSeq[dataStart+i];
	uint32_t bitSize = *headLen;
	//cout << "start pos: " << dataStart << endl;
	//cout << "bit size: " << *headLen << endl;

	// ֻ��һ��val���ļ�: map 12 | bitsize 4 | bitseq
	if (valToWeight.size() == 1)
	{
		vector<char> r(bitSeq.begin() + 16, bitSeq.end());
		vector<char> cs = BitToChar(r, bitSize);
		unordered_map<int, uint32_t>::iterator it = valToWeight.begin();
		vector<int> res(cs.size(), it->first);
		return res;
	}

	BuildTree();
	SetCodeTable();

	vector<char> dataSeq(bitSeq.begin()+dataStart+4, bitSeq.end());
	vector<char> charSeq = BitToChar(dataSeq, bitSize); // ��תchar

	vector<int> result;
	HuffmanNode* ptr = treeRoot;
	
	for (int i = 0; i < bitSize; i++)
	{

		if (charSeq[i] == '0')
			ptr = ptr->left;
		else if (charSeq[i] == '1')
			ptr = ptr->right;

		if (ptr->left == nullptr && ptr->right == nullptr) // �õ�һ��val
		{
			result.push_back(ptr->val);
			ptr = treeRoot;
		}
	}

	return result;
}