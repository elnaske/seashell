#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>

int main() {
    char *line = NULL;
    size_t len = 0;
    
    while (1) {
        printf("seashell> ");
        
        ssize_t n_read = getline(&line, &len, stdin);

        if (n_read == -1) {
            return -1;
        }
        if (strncmp(line, "exit\n", len) == 0) {
            break;
        }

        printf("%s", line);
    }

    free(line);
    return 0;
}