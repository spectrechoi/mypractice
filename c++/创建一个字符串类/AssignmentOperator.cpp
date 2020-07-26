/*******************************************************************
Copyright(c) 2016, Harry He
All rights reserved.

Distributed under the BSD license.
(See accompanying file LICENSE.txt at
https://github.com/zhedahht/CodingInterviewChinese2/blob/master/LICENSE.txt)
*******************************************************************/

//==================================================================
// 《剑指Offer——名企面试官精讲典型编程题》代码
// 作者：何海涛
//==================================================================

// 面试题1：赋值运算符函数
// 题目：如下为类型CMyString的声明，请为该类型添加赋值运算符函数。

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

CMyString::CMyString(const CMyString &str)//如果不声明const形参和实参都会调用一次？
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


//重载关键字 operator  把 ‘=’号变成了一个函数 
//所以str2 = str1 实际上调用的是这个函数 str2.=(str1)
CMyString& CMyString::operator = (const CMyString& str)
{
    if(this == &str){
        printf("self...\r\n");
       return *this; //如果是自己给自己赋值 调用下面会出问题
                    //因为把自己删除了就没了怎么赋值？
    }
        
    printf("%s", m_pData);//打印不出内容 删除的应该是本身
    //分配新内存空间之前要删除原来的内存空间否则会导致内存泄漏
    delete []m_pData;
    m_pData = nullptr;
    //这一步可能会出现删除了后没空间分配 可以先new判断是否为空再操作
    //或者创建一个临时的 保存m_pData 如果内存分配成功了再copy 失败了就不动
    m_pData = new char[strlen(str.m_pData) + 1];
    
    strcpy(m_pData, str.m_pData);

    return *this;//没返回引用不能连续赋值
}

// ====================测试代码====================
void CMyString::Print()
{
    printf("%s", m_pData);
}

void Test1()
{
    printf("Test1 begins:\n");

    char* text = "Hello world";

    CMyString str1(text);//调用构造函数 并且传递值
    CMyString str2;//调用构造函数传递空值
    str2 = str1;//调用赋值

    printf("The expected result is: %s.\n", text);

    printf("The actual result is: ");
    str2.Print();
    printf(".\n");
}

// 赋值给自己
void Test2()
{
    printf("Test2 begins:\n");

    char* text = "Hello world";

    CMyString str1(text);//调用构造函数
    str1 = str1;//调用赋值 给自己赋值不处理直接返回

    printf("The expected result is: %s.\n", text);

    printf("The actual result is: ");
    str1.Print();
    printf(".\n");
}

// 连续赋值
void Test3()
{
    printf("Test3 begins:\n");

    char* text = "Hello world";

    CMyString str1(text);//调用构造函数
    CMyString str2, str3;////调用构造函数传递空值
    str3 = str2 = str1;//连续赋值 1->2 2->3(应该) 赋值前删掉类里的内容？

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

