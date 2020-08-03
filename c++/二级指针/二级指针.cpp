#include <cstdio>


void get_list(int *list)
{
    //list形参 所以*list是个形参指针 list的值是p指向的地址复制过来的 
    list = malloc(xxx); //malloc返回的是地址 list是形参指针  改变list指向的地址并不会改变p的值
}
int *p
get_list(p) //复制p指向的地址p传递

void get_list_1(int **list)//形参是个二级指针 list保存的是p的地址 *list保存的是p指向的地址 **list是p指向的地址的值
{
    list = malloc(xxx); //malloc返回的是地址  改变形参保存的地址没有意义
    *list = malloc(xxx); //malloc返回的是地址  改变p指向的地址可以让p得到malloc得到的内存
}
int *p
get_list(&p) //复制的是p的地址

int main(void)
{
    int a = 10; // a的地址 0x1020   地址0x1020的值为10
    int *p = &a;// p的地址&p = 0x1030 p指向的地址为a的地址 p=0x1020  p指向的地址的值 *p =  10,  
    int **q = &p; // q的地址为 =0x1040 q指向的地址为p的地址 q=0x1030 q指向的地址的值为p的值为 *q=0x1020  *p=10

    get_list(p);//传的是p 是p保存的地址
    get_list(&p);//传的是p的地址
    //q   = &p
    //*q  = p   = &a
    //**q = *p  = a
}

