#include <iostream>

using namespace std;




class A
{
	private:
		int value;
		
	public:
		A(int n) //���캯�� ���󴴽���ʱ������һ�� 
		{
			value = n;
			std::cout<<"init"<<value<<std::endl;
		}
		A(const A& other)
		{
			value = other.value;
			std::cout<<"init other"<<std::endl;
		}
		~A() //��������  ���󱻻��յ�ʱ������һ�� 
		{
			std::cout<<"delete"<<std::endl;
		}
		
		void Print(){
			std::cout<<value<<std::endl;
		}
};

int main()
{
	A a = 10;//ִ��A ��int n�� a=10 �൱�� ִ�� A a(10); 
	A b = a;//ִ��A ��const..�� 	�൱�� ִ�� A a(a); 	
	b.Print();
	
	return 0;
}


