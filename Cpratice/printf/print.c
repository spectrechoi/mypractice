#include <stdio.h>

int main()
{
	printf("hello word!\r\n");
	//printf("\033[47;31mhello world!!\033[5m");
	printf("\r\nhello word!!!\r\n");
	printf("\033[?25l\033[2J");
	printf("\r\nhello word!!!!\r\n");
	
	return 0;
}
