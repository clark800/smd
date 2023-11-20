#include <stdio.h>
#include <stdlib.h>

static int depth = 0;
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

void closeBlocks(char index) {
    for (unsigned char i = 0; i < length - index; i++) {
        if (stack[length - i - 1] == '>')
            fputs("</blockquote>\n", output);
        else if (stack[length - i - 1] == '*')
            fputs("</li>\n</ul>\n", output);
    }
    length = index;
}

char* beginBlock() {
    unsigned char level = 0;
    char* line = getLine(input, 0);
    if (line == NULL) {
        closeBlocks(0);
        return NULL;
    }
    // first check how much of the stack is preserved and close blocks
    for (; level < length; level++) {
        if (stack[level] == '>') {
            if (line[0] == '>' && line[1] == ' ') {
                line += 2;
            } else {
                break;
            }
        } else if (stack[level] == '*') {
            if (line[0] == '*' && line[1] == ' ') {
                closeBlocks(++level);
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
    closeBlocks(level);

    // finally we add new stack elements still remaining in line
    for (;; line += 2) {
        if (line[0] == '>' && line[1] == ' ') {
            fputs("<blockquote>\n", output);
            stack[length++] = '>';
        } else if (line[0] == '*' && line[1] == ' ') {
            fputs("<ul>\n<li>\n", output);
            stack[length++] = '*';
        } else {
            break;
        }
    }
    return line;
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
