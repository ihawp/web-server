#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "program_speed.h"
#include "string_view.h"

#define BUFFER_SIZE 256

// 0x0A = \n
// 0x0D = \r

int get_line(
    FILE *fp,
    char *buffer
) {
    for (int i = 0; i < BUFFER_SIZE - 1; i++) {

        int ntc = fgetc(fp);

        if (ntc == EOF) break;
        if ((char)ntc == 0x0D) {

            char tc = fgetc(fp); // move forward 1 char to find \n?
            if (tc == EOF) break;

            // if fgetc doesn't return 0x0A we have moved to far without doing anything
            if ((char)tc == 0x0A) {
                buffer[i] = 0x00; // put null terminator on end
                return 0;
            } else {
                int r = fseek(fp, -1, SEEK_CUR); // go backward -1 char if \n not found
                if (r != 0) return -1;
            }
        }

        buffer[i] = (char)ntc;
    }

    return -1;
}

int main() {

    FILE *fp = fopen("users.csv", "r");
    if (fp == NULL) {
        printf("Failed to open file");
        exit(EXIT_FAILURE);
        return 1;
    }

    char *line_buffer = malloc(BUFFER_SIZE);
    if (line_buffer == NULL) {
        printf("Failed to allocated memory for buffer");
        exit(EXIT_FAILURE);
        return 1;
    }

    ProgramSpeed speed;
    start(&speed);

    while (get_line(fp, line_buffer) == 0) {
        printf("LINE: %s\n", line_buffer);
        memset(line_buffer, '\0', BUFFER_SIZE);
    }

    end(&speed);

    free(line_buffer);
    fclose(fp);

    return 0;
}