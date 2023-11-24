#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "read.h"
#include "block.h"

static char stack[256] = {0};
static unsigned char depth = 0;
static FILE *INPUT = NULL;

typedef struct {
    char *open, *prefix, *indent, *opentags, *reopentags, *closetags;
} Container;

Container containers[] = {
    {"> ", ">", " ", "<blockquote>\n", NULL, "</blockquote>\n"},
    {"/// ", "", "    ", "<aside>\n", NULL, "</aside>\n"},
    {"* ", "", "  ", "<ul>\n<li>\n", "</li>\n<li>\n", "</li>\n</ul>\n"},
    {"- ", "", "  ", "<ul>\n<li>\n", "</li>\n<li>\n", "</li>\n</ul>\n"},
    {"+ ", "", "  ", "<ul>\n<li>\n", "</li>\n<li>\n", "</li>\n</ul>\n"},
    {"0. ", "", "   ", "<ol>\n<li>\n", "</li>\n<li>\n", "</li>\n</ol>\n"}
};

static Container getContainer(char c) {
    switch (c) {
        case '>': return containers[0];
        case '/': return containers[1];
        case '*': return containers[2];
        case '-': return containers[3];
        case '+': return containers[4];
        default: return isdigit(c) ? containers[5] : (Container){0};
    }
}

static inline int startsWith(char* string, char* prefix) {
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

static char* skipContinuationPrefixes(char* line) {
    for (int i = 0; i < depth; i++) {
        Container container = getContainer(stack[i]);
        if (startsWith(line, container.prefix))
            line += strlen(container.prefix);
        if (startsWith(line, container.indent))
            line += strlen(container.indent);
    }
    return line;
}

char* readLine(void) {
    return skipContinuationPrefixes(getLine(INPUT, 0));
}

char* peekLine(void) {
    return skipContinuationPrefixes(getLine(INPUT, 1));
}

int peek(void) {
    char* line = peekLine();
    return line ? line[0] : EOF;
}

static int isBlockOpener(char* line, Container container) {
    if (isdigit(container.open[0]))
        return isdigit(line[0]) && startsWith(line + 1, container.open + 1);
    return startsWith(line, container.open);
}

static char* openBlocks(char* line, FILE* output) {
    while (line) {
        Container container = getContainer(line[0]);
        if (!container.open || !isBlockOpener(line, container))
            return line;
        fputs(container.opentags, output);
        stack[depth++] = container.open[0];
        line += strlen(container.open);
    }
    return line;
}

static int getContinuationPrefixLength(char* line, Container container) {
    if (!startsWith(line, container.prefix))
        return -1;
    int length = strlen(container.prefix);
    if (line[length] == '\n' || line[length] == '\r')
        return length;
    if (startsWith(line + length, container.indent))
        return length + strlen(container.indent);
    return -1;
}

static void closeLevel(char index, FILE* output) {
    for (unsigned char i = 0; i < depth - index; i++)
        fputs(getContainer(stack[depth - i - 1]).closetags, output);
    depth = index;
}

static char* closeBlocks(char* line, FILE* output) {
    if (line == NULL) {
        closeLevel(0, output);
        return NULL;
    }
    for (unsigned char level = 0; level < depth; level++) {
        Container container = getContainer(stack[level]);
        if (container.reopentags && isBlockOpener(line, container)) {
            closeLevel(level + 1, output);
            fputs(container.reopentags, output);
            return line + strlen(container.open);
        }
        int length = getContinuationPrefixLength(line, container);
        if (length < 0) {
            closeLevel(level, output);
            return line;
        }
        line += length;
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
