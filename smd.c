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

static int isBlank(char* line) {
    return line[strspn(line, " \t")] == '\n';
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

static char* processLink(char* start, FILE* output) {
    char* title = start + 1;
    char* titleEnd = strchr(title, ']');
    if (titleEnd == NULL || titleEnd == title || titleEnd[1] != '(')
        return start;
    char* href = titleEnd + 2;
    char* hrefEnd = strchr(href, ')');
    if (hrefEnd == NULL || hrefEnd == href)
        return start;
    fputs("<a href=\"", output);
    fwrite(href, sizeof(char), hrefEnd - href, output);
    fputs("\">", output);
    fwrite(title, sizeof(char), titleEnd - title, output);
    fputs("</a>", output);
    return hrefEnd + 1;
}

static char* processImage(char* start, FILE* output) {
    if (start[1] != '[')
        return start;
    char* title = start + 2;
    char* titleEnd = strchr(title, ']');
    if (titleEnd == NULL || titleEnd == title || titleEnd[1] != '(')
        return start;
    char* href = titleEnd + 2;
    char* hrefEnd = strchr(href, ')');
    if (hrefEnd == NULL || hrefEnd == href)
        return start;
    fputs("<img src=\"", output);
    fwrite(href, sizeof(char), hrefEnd - href, output);
    fputs(" alt=\"", output);
    fwrite(title, sizeof(char), titleEnd - title, output);
    fputs("\">", output);
    return hrefEnd + 1;
}

static void processLine(char* line, FILE* output) {
    char* p = line;
    while (*p != 0) {
        char* brk = strpbrk(p, "![");
        if (brk == NULL) {
            fputs(p, output);
            return;
        }
        fwrite(p, sizeof(char), brk - p, output);
        switch (*brk) {
            case '[': p = processLink(brk, output); break;
            case '!': p = processImage(brk, output); break;
        }
        if (p == brk)
            fputc(*p++, output);
    }
}

static void processHeading(char* line, FILE* output) {
    size_t level = strspn(line, "#");
    size_t indent = strspn(line + level, " \t");
    char* title = line + level + indent;
    if (level > 6 || title[0] == '\n') {
        fputs(LINE, output);
        return;
    }
    char openTag[] = "<h0>";
    char closeTag[] = "</h0>\n";
    openTag[2] = '0' + level;
    closeTag[3] = '0' + level;
    fputs(openTag, output);
    fputs(chomp(title), output);
    fputs(closeTag, output);
}

static void processParagraph(FILE* input, FILE* output) {
    processLine(LINE, output);
    while(!feof(input) && !isBlank(readLine(input)))
        processLine(LINE, output);
}

static void processHTML(FILE* input, FILE* output) {
    fputs(LINE, output);
    while(!feof(input) && !isBlank(readLine(input)))
        fputs(LINE, output);
}

static void processBlock(FILE* input, FILE* output) {
    while(!feof(input)) {
        readLine(input);
        size_t indent = strspn(LINE, " \t");
        if (LINE[0] == '<') {
            processHTML(input, output);
        } else if (LINE[0] == '#') {
            processHeading(LINE, output);
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
