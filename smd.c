#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char LINE[4096];

static char* chomp(char* line) {
    size_t length = strlen(line);
    if (line[length - 1] == '\n')
        line[length - 1] = '\0';
    return line;
}

static char* readLine(FILE* input) {
    LINE[sizeof(LINE) - 1] = '\n';
    fgets(LINE, sizeof(LINE), input);
    if (LINE[sizeof(LINE) - 1] != '\n') {
        fputs("Line too long", stderr);
        exit(1);
    }
    return LINE;
}

static void processLine(char* line, FILE* output) {
    fputs(line, output);
}

static void processParagraph(FILE* input, FILE* output) {
    fputs(LINE, output);
    while(!feof(input)) {
        readLine(input);
        size_t indent = strspn(LINE, " \t");
        if (LINE[indent] == '\n')
            return;
        processLine(LINE, output);
    }
}

static void processBlock(FILE* input, FILE* output) {
    while(!feof(input)) {
        readLine(input);
        size_t indent = strspn(LINE, " \t");
        if (LINE[0] == '<') {
            processParagraph(input, output); // todo: literal mode
        } else if (LINE[0] == '-') {
            fputs("<hr>\n", output);
        } else if (LINE[indent] == '\n') {
        } else {
            int next = fgetc(input);
            ungetc(next, input);
            if (next == '=') {
                fputs("<h1>", output);
                processLine(chomp(LINE), output);
                fputs("</h1>\n", output);
                readLine(input);
            } else if (next == '-') {
                fputs("<h2>", output);
                processLine(chomp(LINE), output);
                fputs("</h2>\n", output);
                readLine(input);
            } else {
                fputs("<p>\n", output);
                processParagraph(input, output);
                fputs("</p>\n", output);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    while(!feof(stdin))
        processBlock(stdin, stdout);
}
