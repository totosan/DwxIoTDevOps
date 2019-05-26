#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>


char** getToken(char* buffer, int buf_len, const char* seperator)
{
    if (buffer[0] != '\0')
    {
        char* buffer2 = strdup(buffer);

        char** ch = (char**)malloc(10 * sizeof(char *));
        int i = 0;
        ch[i] = (char*)malloc(20 * sizeof(char));
        ch[i] = strtok(buffer2, seperator);
        while (ch[i] != NULL)
        {
            printf("token: %s\r\n", ch[i]);
            ch[++i] = strtok(NULL, seperator);
        }

        return ch;
    }
}
