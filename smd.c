#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char LINE[4096];

static int peek(FILE* input) {
    int c = fgetc(input);
    return ungetc(c, input);
}

static char* chomp(char* line) {
    size_t length = strlen(line);
    if (line[length - 1] == '\n')
        line[length - 1] = '\0';
    return line;
}

static int startsWith(char* string, char* prefix) {
    return string != NULL && strncmp(prefix, string, strlen(prefix)) == 0;
}

static int isBlank(char* line) {
    return line == NULL || line[strspn(line, " \t")] == '\n';
}

static char* readLine(FILE* input) {
    LINE[sizeof(LINE) - 1] = '\n';
    char* result = fgets(LINE, sizeof(LINE), input);
    if (LINE[sizeof(LINE) - 1] != '\n') {
        fputs("Line too long", stderr);
        exit(1);
    }
    return result;
}

static char* processSimpleLink(char* start, FILE* output) {
    char* href = start + 1;
    char* hrefEnd = strchr(href, '>');
    if (hrefEnd == NULL || hrefEnd == href)
        return start;
    fputs("<a href=\"", output);
    fwrite(href, sizeof(char), hrefEnd - href, output);
    fputs("\">", output);
    fwrite(href, sizeof(char), hrefEnd - href, output);
    fputs("</a>", output);
    return hrefEnd + 1;
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

static char* processCode(char* start, FILE* output) {
    char delimiter[] = "```";
    size_t span = strspn(start, "`");
    size_t length = span < 3 ? span : 3;
    delimiter[length] = '\0';
    char* code = start + length;
    char* end = strstr(code, delimiter);
    if (end == NULL)
        return start;
    fputs("<code>", output);
    fwrite(code, sizeof(char), end - code, output);
    fputs("</code>", output);
    return end + length;
}

static char* processBackslash(char* start, FILE* output) {
    char* p = start;
    fputc(*p++, output);
    if (*p != 0)
        fputc(*p++, output);
    return p;
}

static void processLine(char* line, FILE* output) {
    char* p = line;
    while (*p != 0) {
        char* brk = strpbrk(p, "`<![\\");
        if (brk == NULL) {
            fputs(p, output);
            return;
        }
        fwrite(p, sizeof(char), brk - p, output);
        switch (*brk) {
            case '`': p = processCode(brk, output); break;
            case '<': p = processSimpleLink(brk, output); break;
            case '[': p = processLink(brk, output); break;
            case '!': p = processImage(brk, output); break;
            case '\\': p = processBackslash(brk, output); break;
        }
        if (p == brk)
            fputc(*p++, output);
    }
}

static void printHeading(char* title, int level, FILE* output) {
    char openTag[] = "<h0>";
    char closeTag[] = "</h0>\n";
    openTag[2] = '0' + level;
    closeTag[3] = '0' + level;
    fputs(openTag, output);
    fputs(chomp(title), output);
    fputs(closeTag, output);
}

static void processHeading(char* line, FILE* output) {
    size_t level = strspn(line, "#");
    size_t whitespace = strspn(line + level, " \t");
    char* title = line + level + whitespace;
    if (level > 6 || title[0] == '\n') {
        fputs(LINE, output);
    } else {
        printHeading(title, level, output);
    }
}

static void processCodeFence(char* line, FILE* input, FILE* output) {
    char delimiter[] = "````````````````";
    size_t length = strspn(line, "`");
    size_t whitespace = strspn(line + length, " \t");
    if (length < sizeof(delimiter))
        delimiter[length] = '\0';
    char* language = chomp(line + length + whitespace);
    if (language[0] != '\0') {
        fputs("<pre><code class=\"language-", output);
        fputs(language, output);
        fputs("\">\n", output);
    } else {
        fputs("<pre>\n<code>\n", output);
    }
    while(!feof(input) && !startsWith(readLine(input), delimiter))
        fputs(LINE, output);
    fputs("</code>\n</pre>\n", output);
}

static void processUnorderedList(FILE* input, FILE* output, int n) {
    fputs("<ul>\n<li>\n", output);
    for(int i = 0;; i++) {
        int indent = strspn(LINE, " \t");
        if (startsWith(LINE + indent, "* ")) {
            if (indent < n) {
                break;
            } else if (indent > n) {
                processUnorderedList(input, output, indent);
                continue;  // LINE still needs to be processed
            } if (indent == n) {
                if (i != 0)
                    fputs("</li>\n<li>\n", output);
                processLine(LINE + indent + 2, output);
            }
        } else if (!isBlank(LINE)) {
            processLine(LINE + indent, output);
        }
        int next = peek(input);
        if (next != '*' && !isspace(next))  // todo: * could be italic
            break;
        readLine(input);
    }
    fputs("</li>\n</ul>\n", output);
}

static void processBlockquote(char* line, FILE* input, FILE* output) {
    fputs("<blockquote>\n", output);
    do {
        int offset = line[1] == ' ' ? 2 : 1;
        processLine(line + offset, output);
        if (peek(input) != '>')
            break;
    } while(readLine(input));
    fputs("</blockquote>\n", output);
}

static void processParagraph(FILE* input, FILE* output) {
    fputs("<p>\n", output);
    processLine(LINE, output);
    while(!isBlank(readLine(input))) {
        if (startsWith(LINE, "```")) {
            fputs("</p>\n", output);
            processCodeFence(LINE, input, output);
            return;
        } else if (startsWith(LINE, "* ")) {
            fputs("</p>\n", output);
            processUnorderedList(input, output, 0);
            return;
        } else if (startsWith(LINE, ">")) {
            fputs("</p>\n", output);
            processBlockquote(LINE, input, output);
            return;
        } else {
            processLine(LINE, output);
        }
    }
    fputs("</p>\n", output);
}

static void processHTML(FILE* input, FILE* output) {
    fputs(LINE, output);
    while(!isBlank(readLine(input)))
        fputs(LINE, output);
}

static void processFile(FILE* input, FILE* output) {
    while(readLine(input)) {
        size_t indent = strspn(LINE, " \t");
        if (LINE[indent] == '\n') {
        } else if (LINE[0] == '#') {
            processHeading(LINE, output);
        } else if (LINE[0] == '>') {
            processBlockquote(LINE, input, output);
        } else if (startsWith(LINE, "* ")) {
            processUnorderedList(input, output, 0);
        } else if (startsWith(LINE, "---")) {
            fputs("<hr>\n", output);
        } else if (startsWith(LINE, "```")) {
            processCodeFence(LINE, input, output);
        } else {
            int next = peek(input);
            if (next == '=') {
                printHeading(LINE, 1, output);
                readLine(input);
            } else if (next == '-') {
                printHeading(LINE, 2, output);
                readLine(input);
            } else {
                processParagraph(input, output);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    processFile(stdin, stdout);
}
