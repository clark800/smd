#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "read.h"
#include "block.h"

static char stack[256] = {0};
static unsigned char depth = 0;
static FILE *INPUT = NULL;

static int startsWith(char* string, char* prefix) {
    return string != NULL && strncmp(prefix, string, strlen(prefix)) == 0;
}

static char* getLine(FILE* input, int peek) {
    static char peeked = 0, flipped = 0;
    static char bufferA[4096], bufferB[sizeof(bufferA)];
    char* line = flipped ? bufferB : bufferA;
    char* next = flipped ? bufferA : bufferB;
    if (peeked) {
        peeked = peek;
        flipped ^= !peek;
        return next + 1;
    }
    char* buffer = peek ? next : line;
    buffer[0] = '\n';
    buffer[sizeof(bufferA) - 2] = '\n';
    // load buffer with offset so we can safely look back a character
    char* result = fgets(buffer + 1, sizeof(bufferA) - 1, input);
    if (buffer[sizeof(bufferA) - 2] != '\n') {
        fputs("\nError: line too long\n", stderr);
        exit(1);
    }
    peeked = result ? peek : 0;
    return result;
}

static char* skipPrefixes(char* line) {
    for (int i = 0; i < depth; i++) {
        char c = stack[i];
        if (c == '>' && line[0] == c)
            line += line[1] == ' ' ? 2 : 1;
        if (c == '/')
            line += startsWith(line, "    ") ? 4 : (line[0] == '\t' ? 1 : 0);
        if (strchr("*-+", c) && line[0] == ' ' && line[1] == ' ')
            line += 2;
        if (c == '.' && line[0] == ' ' && line[1] == ' ' && line[2] == ' ')
            line += 3;
    }
    return line;
}

char* readLine(void) {
    return skipPrefixes(getLine(INPUT, 0));
}

char* peekLine(void) {
    return skipPrefixes(getLine(INPUT, 1));
}

int peek(void) {
    char* line = peekLine();
    return line ? line[0] : EOF;
}

static char* openBlocks(char* line, FILE* output) {
    while(1) {
        char c = line ? line[0] : 0;
        switch (c) {
            case '>':
                if (startsWith(line, "> ")) {
                    fputs("<blockquote>\n", output);
                    line += 2;
                    break;
                }
                return line;
            case '/':
                if (startsWith(line, "/// ")) {
                    fputs("<aside>\n", output);
                    line += 4;
                    break;
                }
                return line;
            case '*':
            case '-':
            case '+':
                if (line[1] == ' ') {
                    fputs("<ul>\n<li>\n", output);
                    line += 2;
                    break;
                }
                return line;
           default:
                if (isdigit(c) && line[1] == '.' && line[2] == ' ') {
                    c = '.';
                    fputs("<ol>\n<li>\n", output);
                    line += 3;
                    break;
                }
                return line;
        }
        stack[depth++] = c;
    }
}

static void closeLevel(char index, FILE* output) {
    for (unsigned char i = 0; i < depth - index; i++) {
        switch (stack[depth - i - 1]) {
            case '>': fputs("</blockquote>\n", output); break;
            case '/': fputs("</aside>\n", output); break;
            case '*':
            case '-':
            case '+': fputs("</li>\n</ul>\n", output); break;
            case '.': fputs("</li>\n</ol>\n", output); break;
        }
    }
    depth = index;
}

static char* closeBlocks(char* line, FILE* output) {
    unsigned char level = 0;
    if (line == NULL) {
        closeLevel(0, output);
        return NULL;
    }
    for (; level < depth; level++) {
        switch (stack[level]) {
            case '>':
                if (line[0] == stack[level] && strchr(" \r\n", line[1])) {
                    line += line[1] == ' ' ? 2 : 1;
                    continue;
                }
                break;
            case '/':
                if (startsWith(line, "    ") || strchr("\t\r\n", line[0])) {
                    line += line[0] == ' ' ? 4 : (line[0] == '\t' ? 1 : 0);
                    continue;
                }
                break;
            case '*':
            case '-':
            case '+':
                if (line[0] == stack[level] && line[1] == ' ') {
                    closeLevel(++level, output);
                    fputs("</li>\n<li>\n", output);
                    return line + 2;
                } else if (line[0] == ' ' && line[1] == ' ') {
                    line += 2;
                    continue;
                }
                break;
            case '.':
                if (isdigit(line[0]) && line[1] == '.' && line[2] == ' ') {
                    closeLevel(++level, output);
                    fputs("</li>\n<li>\n", output);
                    return line + 3;
                } else if (line[0] == ' ' && line[1] == ' ' && line[2] == ' ') {
                    line += 3;
                    continue;
                }
        }
        closeLevel(level, output);
        return line;
    }
    return line;
}

static char* beginBlock(char* line, FILE* output) {
    return openBlocks(closeBlocks(line, output), output);
}

int main(void) {
    char* line = NULL;
    INPUT = stdin;
    while ((line = beginBlock(getLine(INPUT, 0), stdout)))
        processBlock(line, stdout);
}
