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
    char *open, *premore, *more, *altmore, *opentags, *reopentags, *closetags;
} Container;

Container containers[] = {
    {"> ", ">", " ", "\t\r\n", "<blockquote>\n", NULL, "</blockquote>\n"},
    {"/// ", "", "    ", "\t\r\n", "<aside>\n", NULL, "</aside>\n"},
    {"* ", "", "  ", "\t", "<ul>\n<li>\n", "</li>\n<li>\n", "</li>\n</ul>\n"},
    {"- ", "", "  ", "\t", "<ul>\n<li>\n", "</li>\n<li>\n", "</li>\n</ul>\n"},
    {"+ ", "", "  ", "\t", "<ul>\n<li>\n", "</li>\n<li>\n", "</li>\n</ul>\n"},
    {"0. ", "", "   ", "\t", "<ol>\n<li>\n", "</li>\n<li>\n", "</li>\n</ol>\n"}
};

static Container getContainer(char c) {
    if (isdigit(c))
        c = '0';
    switch (c) {
        case '>': return containers[0];
        case '/': return containers[1];
        case '*': return containers[2];
        case '-': return containers[3];
        case '+': return containers[4];
        case '0': return containers[5];
        default: return (Container){0};
    }
}

static inline int startsWith(char* string, char* prefix) {
    return string != NULL && strncmp(prefix, string, strlen(prefix)) == 0;
}

static int isBlockOpener(char* line, Container container) {
    if (isdigit(container.open[0]))
        return isdigit(line[0]) && startsWith(line + 1, container.open + 1);
    return startsWith(line, container.open);
}

static int getContinuationPrefixLength(char* line, Container container) {
    int length = 0;
    if (!startsWith(line, container.premore))
        return -1;
    length += strlen(container.premore);
    if (startsWith(line + length, container.more))
        return length + strlen(container.more);
    char c = line[length];
    if (strchr(container.altmore, c))
        return length + ((c == '\n' || c == '\r') ? 0 : 1);
    return -1;
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
        if (startsWith(line, container.premore))
            line += strlen(container.premore);
        if (startsWith(line, container.more))
            line += strlen(container.more);
        else if (strchr(container.altmore, line[0]))
            line += (line[0] == '\n' || line[0] == '\r') ? 0 : 1;
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
