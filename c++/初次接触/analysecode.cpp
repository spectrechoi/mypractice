#include <iostream>

using namespace std;




class A
{
	private:
		int value;
		
	public:
		A(int n) //构造函数 对象创建的时候会调用一次 
		{
			value = n;
			std::cout<<"init"<<value<<std::endl;
		}
		A(const A& other)
		{
			value = other.value;
			std::cout<<"init other"<<std::endl;
		}
		~A() //析构函数  对象被回收的时候会调用一次 
		{
			std::cout<<"delete"<<std::endl;
		}
		
		void Print(){
			std::cout<<value<<std::endl;
		}
};

int main()
{
	A a = 10;//执行A （int n） a=10 相当于 执行 A a(10); 
	A b = a;//执行A （const..） 	相当于 执行 A a(a); 	
	b.Print();
	
	return 0;
}


