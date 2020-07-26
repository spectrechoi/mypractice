#include "stdio.h"
#include "log.h"
int main(int argv,char**argc){
	printf("test\n");
	LogWrite("INFO","Hello World!");
	LogWrite("error","H.e.l.l.o W.o.r.l.d!");
	LogWrite("mint","H e l l o W o r l d!");
	LogWrite("iout","Hallo World!");

	return 0;
}
