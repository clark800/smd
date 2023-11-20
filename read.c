#include <stdio.h>
#include <stdlib.h>

static char stack[256] = {0};
static unsigned char length = 0;
static FILE *input = NULL, *output = NULL;

void initContext(FILE* in, FILE* out) {
    input = in;
    output = out;
}

static char* getLine(FILE* input, int peek) {
    static char peeked = 0, flipped = 0;
    static char bufferA[4096], bufferB[4096];
    char* line = flipped ? bufferB : bufferA;
    char* next = flipped ? bufferA : bufferB;
    if (peeked) {
        peeked = peek;
        flipped ^= !peek;
        return next;
    }
    char* buffer = peek ? next : line;
    buffer[4096 - 1] = '\n';
    char* result = fgets(buffer, 4096, input);
    if (buffer[4096 - 1] != '\n') {
        fputs("\nError: line too long", stderr);
        exit(1);
    }
    peeked = result ? peek : 0;
    return result;
}

// skip prefixes that are part of the current block
static char* skipPrefixes(char* line) {
    for (int i = 0; i < length; i++) {
        if (stack[i] == '>' && line[0] == '>')
            line += line[1] == ' ' ? 2 : 1;
        if (stack[i] == '*' && line[0] == ' ')
            line += line[1] == ' ' ? 2 : 1;
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
            case '*':
                if (line[1] != ' ')
                    return line;
                fputs("<ul>\n<li>\n", output);
                line += 2;
                break;
           default: return line;
        }
        stack[length++] = c;
    }
}

void closeLevel(char index) {
    for (unsigned char i = 0; i < length - index; i++) {
        switch (stack[length - i - 1]) {
            case '>': fputs("</blockquote>\n", output); break;
            case '*': fputs("</li>\n</ul>\n", output); break;
        }
    }
    length = index;
}

char* closeBlocks(char* line) {
    unsigned char level = 0;
    if (line == NULL) {
        closeLevel(0);
        return NULL;
    }
    for (; level < length; level++) {
        if (stack[level] == '>') {
            if (line[0] == '>') {
                line += line[1] == ' ' ? 2 : 1;
            } else {
                break;
            }
        } else if (stack[level] == '*') {
            if (line[0] == '*' && line[1] == ' ') {
                closeLevel(++level);
                fputs("</li>\n<li>\n", output);
                line += 2;
                break;
            } else if (line[0] == ' ' && line[1] == ' ') {
                line += 2;
            } else {
                break;
            }
        }
    }
    closeLevel(level);
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
