#include  <stdlib.h>
#include  <stdio.h>

extern char **environ;

int main()
{
        char **env = environ;

        while (*env)
        {
            /* code */
            printf("%s\n",*env);
            env++;
        }

        exit(0);
        
}