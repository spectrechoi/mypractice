#include <cstdio>

//1-判断长度用sizeof/typelen
//2-用第三个数组来放值比较简单 

int str1[] = {0,0,0,0};
int str2[] = {0,2,4,9,6,10}; 
int str3[1024];

void dump(int *data,int len)
{
	int i = 0;
	while(i < len && (data+i)!= NULL) 
	{
		printf("%d ",*(data+i));
		i++;
	}
	printf("\r\n");
}

void  merge(int *src,int s_size,int *des,int d_size)
{
	int space = s_size + d_size;
	int tail_index = space - 1;
	int des_index = d_size - 1;
	int src_index = s_size -1;
	
	if(space > 1024)
	{
		printf("over flow\r\n");
		return;
	}
	if(src == NULL)
	{
		printf("NULL src!\r\n");
		return;		
	}
	if(des == NULL)
	{
		dump(src,s_size); 
		return;		
	}

//int str1[1024] = {1,3,3,4,5,8,9,9};
//int str2[1024] = {2,2,2,3,4,6,7,8,9}; 	
	
	while(tail_index>=0)
	{
		printf("%d %d\r\n",src[src_index],des[des_index]);
		if(src[src_index] >= des[des_index])
		{
			str3[tail_index] = src[src_index];
			src_index--;
		}
		else
		{
			str3[tail_index] = des[des_index];
			des_index--;
		}
		tail_index--;
		if(src_index < 0 )
		{
			str3[tail_index] = des[des_index];
		}
		if(des_index < 0 )
		{
			str3[tail_index] = src[des_index];
		} 	
		
		dump(str3,space);
	}
	dump(str3,space);
		
		
}


void test1(void)
{
	int i = 0,j = 0;
	
	i = sizeof(str1)/sizeof(int);
	j = sizeof(str2)/sizeof(int);
	printf("i %d j %d\r\n",i,j);
	
	if(i>j)
	{
		merge(str1,i,str2,j);
	}
	else
	{
		merge(str2,j,str1,i);
	}
		
}

int main()
{
	test1();
	
	return 0;
}


