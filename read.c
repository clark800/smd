#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char stack[256] = {0};
static unsigned char depth = 0;
static FILE *input = NULL, *output = NULL;

void initContext(FILE* in, FILE* out) {
    input = in;
    output = out;
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
        if (c == '>' && line[0] == '>')
            line += line[1] == ' ' ? 2 : 1;
        if (c == '.' && line[0] == ' ' && line[1] == ' ' && line[2] == ' ')
            line += 3;
        if (strchr("*-+", c) && line[0] == ' ' && line[1] == ' ')
            line += 2;
    }
    return line;
}

char* openBlocks(char* line) {
    while(1) {
        char c = line ? line[0] : 0;
        switch (c) {
            case '>':
                fputs("<blockquote>\n", output);
                line += line[1] == ' ' ? 2 : 1;
                break;
            case '-':
            case '+':
            case '*':
                if (line[1] != ' ')
                    return line;
                fputs("<ul>\n<li>\n", output);
                line += 2;
                break;
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

void closeLevel(char index) {
    for (unsigned char i = 0; i < depth - index; i++) {
        switch (stack[depth - i - 1]) {
            case '>': fputs("</blockquote>\n", output); break;
            case '-':
            case '+':
            case '*': fputs("</li>\n</ul>\n", output); break;
            case '.': fputs("</li>\n</ol>\n", output); break;
        }
    }
    depth = index;
}

char* closeBlocks(char* line) {
    unsigned char level = 0;
    if (line == NULL) {
        closeLevel(0);
        return NULL;
    }
    for (; level < depth; level++) {
        switch (stack[level]) {
            case '>':
                if (line[0] == stack[level]) {
                    line += line[1] == ' ' ? 2 : 1;
                    continue;
                }
                break;
            case '.':
                if (isdigit(line[0]) && line[1] == '.' && line[2] == ' ') {
                    closeLevel(++level);
                    fputs("</li>\n<li>\n", output);
                    return line + 3;
                } else if (line[0] == ' ' && line[1] == ' ' && line[2] == ' ') {
                    line += 3;
                    continue;
                }
            case '-':
            case '+':
            case '*':
                if (line[0] == stack[level] && line[1] == ' ') {
                    closeLevel(++level);
                    fputs("</li>\n<li>\n", output);
                    return line + 2;
                } else if (line[0] == ' ' && line[1] == ' ') {
                    line += 2;
                    continue;
                }
                break;
        }
        closeLevel(level);
        return line;
    }
    return line;
}

char* beginBlock() {
    return openBlocks(closeBlocks(getLine(input, 0)));
}

char* peekLine() {
    return skipPrefixes(getLine(input, 1));
}

int peek() {
    char* line = peekLine();
    return line ? line[0] : EOF;
}

char* readLine() {
    return skipPrefixes(getLine(input, 0));
}
