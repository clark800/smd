#include <stdio.h>
#include <stdlib.h>

static int depth = 0;
static FILE *input, *output;

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

// strip up to 'depth' levels of blockquote
static char* skipBlockquote(char* line) {
    for (int i = 0; line && line[0] == '>' && i < depth; i++)
        line += (line[1] == ' ') ? 2 : 1;
    return line;
}

static int getDepth(char* line) {
    int d = 0;
    for (; line && *line == '>'; d++)
        line += (line[1] == ' ') ? 2 : 1;
    return d;
}

char* beginBlock() {
    char* line = getLine(input, 0);
    int newDepth = getDepth(line);
    if (newDepth > depth)
        for (int i = 0; i < newDepth - depth; i++)
            fputs("<blockquote>\n", output);
    if (newDepth < depth)
        for (int i = 0; i < depth - newDepth; i++)
            fputs("</blockquote>\n", output);
    depth = newDepth;
    return skipBlockquote(line);
}

char* peekLine() {
    return skipBlockquote(getLine(input, 1));
}

int peek() {
    char* line = peekLine();
    return line ? line[0] : EOF;
}

char* readLine() {
    return skipBlockquote(getLine(input, 0));
}
