#include  <stdio.h>
#include <unistd.h>
#include <stdlib.h>

int main(int argc ,char *argv[])
{
    int opt;

    while((opt = getopt(argc, argv, ":if:lr::")) != -1){
        switch (opt)
        {
                 case 'i':
                 case 'l':
                 case 'r':
                    printf("option : %c\n",opt);
                 break;

                 case 'f':
                    printf("filename:  %s\n",optarg);
                 break;

                 case ':':
                 printf("option needs a value\n");
                 break;

                 case '?':
                    printf("unknow opt: %c\n",optopt);
                 break;                   
        
        default:
            break;
        }

    }
for(; optind < argc; optind++)
{
    printf("argu: %s\n",argv[optind]);    
}
exit(0);

}