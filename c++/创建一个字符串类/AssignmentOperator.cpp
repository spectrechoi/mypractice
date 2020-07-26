/*******************************************************************
Copyright(c) 2016, Harry He
All rights reserved.

Distributed under the BSD license.
(See accompanying file LICENSE.txt at
https://github.com/zhedahht/CodingInterviewChinese2/blob/master/LICENSE.txt)
*******************************************************************/

//==================================================================
// ����ָOffer�����������Թپ������ͱ���⡷����
// ���ߣ��κ���
//==================================================================

// ������1����ֵ���������
// ��Ŀ������Ϊ����CMyString����������Ϊ��������Ӹ�ֵ�����������

#include<cstring>
#include<cstdio>

#define nullptr NULL

class CMyString
{
public:
    CMyString(char* pData = nullptr);
    CMyString(const CMyString& str);
    ~CMyString(void);

    CMyString& operator = (const CMyString& str);

    void Print();
      
private:
    char* m_pData;
};

CMyString::CMyString(char *pData)
{
    if(pData == nullptr)
    {
        m_pData = new char[1];
        m_pData[0] = '\0';
        printf("null...\r\n");
    }
    else
    {
        int length = strlen(pData);
        m_pData = new char[length + 1];
        strcpy(m_pData, pData);
        printf("init one...\r\n");
    }
}

CMyString::CMyString(const CMyString &str)//���������const�βκ�ʵ�ζ������һ�Σ�
{
    int length = strlen(str.m_pData);
    m_pData = new char[length + 1];
    strcpy(m_pData, str.m_pData);
    printf("get it to new...\r\n");
}

CMyString::~CMyString()
{
    printf("end...\r\n");
    delete[] m_pData;
}


//���عؼ��� operator  �� ��=���ű����һ������ 
//����str2 = str1 ʵ���ϵ��õ���������� str2.=(str1)
CMyString& CMyString::operator = (const CMyString& str)
{
    if(this == &str){
        printf("self...\r\n");
       return *this; //������Լ����Լ���ֵ ��������������
                    //��Ϊ���Լ�ɾ���˾�û����ô��ֵ��
    }
        
    printf("%s", m_pData);//��ӡ�������� ɾ����Ӧ���Ǳ���
    //�������ڴ�ռ�֮ǰҪɾ��ԭ�����ڴ�ռ����ᵼ���ڴ�й©
    delete []m_pData;
    m_pData = nullptr;
    //��һ�����ܻ����ɾ���˺�û�ռ���� ������new�ж��Ƿ�Ϊ���ٲ���
    //���ߴ���һ����ʱ�� ����m_pData ����ڴ����ɹ�����copy ʧ���˾Ͳ���
    m_pData = new char[strlen(str.m_pData) + 1];
    
    strcpy(m_pData, str.m_pData);

    return *this;//û�������ò���������ֵ
}

// ====================���Դ���====================
void CMyString::Print()
{
    printf("%s", m_pData);
}

void Test1()
{
    printf("Test1 begins:\n");

    char* text = "Hello world";

    CMyString str1(text);//���ù��캯�� ���Ҵ���ֵ
    CMyString str2;//���ù��캯�����ݿ�ֵ
    str2 = str1;//���ø�ֵ

    printf("The expected result is: %s.\n", text);

    printf("The actual result is: ");
    str2.Print();
    printf(".\n");
}

// ��ֵ���Լ�
void Test2()
{
    printf("Test2 begins:\n");

    char* text = "Hello world";

    CMyString str1(text);//���ù��캯��
    str1 = str1;//���ø�ֵ ���Լ���ֵ������ֱ�ӷ���

    printf("The expected result is: %s.\n", text);

    printf("The actual result is: ");
    str1.Print();
    printf(".\n");
}

// ������ֵ
void Test3()
{
    printf("Test3 begins:\n");

    char* text = "Hello world";

    CMyString str1(text);//���ù��캯��
    CMyString str2, str3;////���ù��캯�����ݿ�ֵ
    str3 = str2 = str1;//������ֵ 1->2 2->3(Ӧ��) ��ֵǰɾ����������ݣ�

    printf("The expected result is: %s.\n", text);

    printf("The actual result is: ");
    str2.Print();
    printf(".\n");

    printf("The expected result is: %s.\n", text);

    printf("The actual result is: ");
    str3.Print();
    printf(".\n");
}

int main(int argc, char* argv[])
{
    Test1();
    Test2();
    Test3();

    return 0;
}

