#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

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

static char* skipWhitespace(char* line) {
    return line + strspn(line, " \t");
}

static int startsWith(char* string, char* prefix) {
    return string != NULL && strncmp(prefix, string, strlen(prefix)) == 0;
}

static int isBlank(char* line) {
    return line == NULL || line[strspn(line, " \t")] == '\n';
}

static size_t fputr(char* start, char* end, FILE* output) {
    return fwrite(start, sizeof(char), end - start, output);
}

static char* readLine(FILE* input) {
    static char line[4096];
    line[sizeof(line) - 1] = '\n';
    char* result = fgets(line, sizeof(line), input);
    if (line[sizeof(line) - 1] != '\n') {
        fputs("Line too long", stderr);
        exit(1);
    }
    return result;
}

static char* processAmpersand(char* start, FILE* output) {
    char* semicolon = strchr(start, ';');
    char* space = memchr(start, ' ', semicolon - start);
    if (semicolon != NULL && space == NULL && semicolon > start + 1) { // Entity
        fputr(start, semicolon + 1, output);
        return semicolon + 1;
    }
    fputs("&amp;", output);
    return start + 1;
}

static char* processEscape(char* start, FILE* output) {
    switch (start[0]) {
        case '<': fputs("&lt;", output); break;
        case '>': fputs("&gt;", output); break;
        case '&': return processAmpersand(start, output);
    }
    return start + 1;
}

static char* processLessThan(char* start, FILE* output) {
    char* content = start + 1;
    if (isspace(content[0]))
        return processEscape(start, output);
    char* end = strchr(content, '>');
    if (end == NULL || end == content)
        return processEscape(start, output);
    char* space = memchr(content, ' ', end - content);
    char* colon = memchr(content, ':', end - content);
    if (space != NULL || colon == NULL) {  // assume this is an HTML tag
        fputr(start, end + 1, output);
        return end + 1;
    }
    fputs("<a href=\"", output);
    fputr(content, end, output);
    fputs("\">", output);
    fputr(content, end, output);
    fputs("</a>", output);
    return end + 1;
}

static char* findClose(char* p, char* brackets) {
    for (int count = 1; count > 0; count += (*p == *brackets ? 1 : -1))
        if (!(p = strpbrk(++p, brackets)))
            return NULL;
    return p;
}

static char* processLink(char* start, FILE* output) {
    char* title = start + 1;
    char* titleEnd = strchr(title, ']');
    if (titleEnd == NULL || titleEnd == title)
        return start;
    if (title[0] == '^') {
        fputs("<sup><a href=\"#", output);
        fputr(title + 1, titleEnd, output);
        fputs("\">*</sup>", output);
        return titleEnd + 1;
    }
    char* paren = titleEnd + 1;
    if (paren[0] != '(')
        return start;
    char* href = paren + 1;
    char* hrefEnd = findClose(paren, "()");
    if (hrefEnd == NULL || hrefEnd == href)
        return start;
    fputs("<a href=\"", output);
    fputr(href, hrefEnd, output);
    fputs("\">", output);
    fputr(title, titleEnd, output);
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
    fputr(href, hrefEnd, output);
    fputs(" alt=\"", output);
    fputr(title, titleEnd, output);
    fputs("\">", output);
    return hrefEnd + 1;
}

static char* processBackslash(char* start, FILE* output) {
    char* p = start;
    fputc(*p++, output);
    if (*p != 0)
        fputc(*p++, output);
    return p;
}

static char* processVerbatim(char* start, char* end, FILE* output) {
    char* p = start;
    while (p < end) {
        char* brk = strpbrk(p, "<>&");
        if (brk == NULL || brk > end)
            brk = end;
        fputr(p, brk, output);
        switch (brk[0]) {
            case '<': fputs("&lt;", output); break;
            case '>': fputs("&gt;", output); break;
            case '&': fputs("&amp;", output); break;
        }
        p = brk + 1;
    }
    return end;
}

static char* processWrap(char* start, char* wrap, int tightbits,
        char* openTags[], char* closeTags[], FILE* output) {
    size_t maxlen = strlen(wrap);
    char search[] = {wrap[0], '\0'};
    size_t length = strspn(start, search);
    if (length > maxlen || length > 15) {
        fputr(start, start + length, output);
        return start + length;
    }
    char delimiter[16];
    strncpy(delimiter, wrap, length);
    delimiter[length] = '\0';
    char* content = start + length;
    char* end = strstr(content, delimiter);
    int tight = tightbits & (1 << (length - 1));
    if (end == NULL || (tight && (isspace(content[0]) || isspace(end[-1]))))
        return start;
    fputs(openTags[length-1], output);
    processVerbatim(content, end, output);
    fputs(closeTags[length-1], output);
    return end + length;
}

static char* processCode(char* start, FILE* output) {
    char* openTags[] = {"<code>", "<code>", "<code>"};
    char* closeTags[] = {"</code>", "</code>", "</code>"};
    return processWrap(start, "```", 0, openTags, closeTags, output);
}

static char* processAsterisk(char* start, FILE* output) {
    char* openTags[] = {"<em>", "<strong>", "<em><strong>"};
    char* closeTags[] = {"</em>", "</strong>", "</strong></em>"};
    return processWrap(start, "***", 7, openTags, closeTags, output);
}

static char* processMath(char* start, FILE* output) {
    char* openTags[] = {"\\(", "\\["};
    char* closeTags[] = {"\\)", "\\]"};
    return processWrap(start, "$$", 1, openTags, closeTags, output);
}

static void processLine(char* line, FILE* output) {
    char* p = line;
    while (*p != 0) {
        char* brk = strpbrk(p, "`$*<>&![\\");
        if (brk == NULL) {
            fputs(p, output);
            return;
        }
        fputr(p, brk, output);
        switch (*brk) {
            case '`': p = processCode(brk, output); break;
            case '$': p = processMath(brk, output); break;
            case '*': p = processAsterisk(brk, output); break;
            case '<': p = processLessThan(brk, output); break;
            case '>': p = processEscape(brk, output); break;
            case '&': p = processEscape(brk, output); break;
            case '!': p = processImage(brk, output); break;
            case '[': p = processLink(brk, output); break;
            case '\\': p = processBackslash(brk, output); break;
        }
        if (p == brk)
            fputc(*p++, output);
    }
}

static void processCodeFence(char* line, FILE* input, FILE* output) {
    char delimiter[] = "````````````````";
    size_t length = strspn(line, "`");
    if (length < sizeof(delimiter))
        delimiter[length] = '\0';
    char* language = chomp(skipWhitespace(line + length));
    if (language[0] != '\0') {
        fputs("<pre>\n<code class=\"language-", output);
        fputs(language, output);
        fputs("\">\n", output);
    } else {
        fputs("<pre>\n<code>\n", output);
    }
    while ((line = readLine(input)) && !startsWith(line, delimiter))
        fputs(line, output);
    fputs("</code>\n</pre>\n", output);
}

static void processUnorderedList(char* line, FILE* input, FILE* output, int n) {
    fputs("<ul>\n<li>\n", output);
    for (int i = 0;; i++) {
        int indent = strspn(line, " \t");
        if (startsWith(line + indent, "* ")) {
            if (indent < n) {
                break;
            } else if (indent > n) {
                processUnorderedList(line, input, output, indent);
                continue;  // line still needs to be processed
            } if (indent == n) {
                if (i != 0)
                    fputs("</li>\n<li>\n", output);
                processLine(line + indent + 2, output);
            }
        } else if (!isBlank(line)) {
            processLine(line + indent, output);
        }
        int next = peek(input);
        if (next != '*' && !isspace(next))  // todo: * could be italic
            break;
        line = readLine(input);
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
    } while ((line = readLine(input)));
    fputs("</blockquote>\n", output);
}

static void processParagraph(char* line, FILE* input, FILE* output) {
    fputs("<p>\n", output);
    processLine(line, output);
    while (!isBlank(line = readLine(input))) {
        if (startsWith(line, "```")) {
            fputs("</p>\n", output);
            processCodeFence(line, input, output);
            return;
        } else if (startsWith(line, "* ")) {
            fputs("</p>\n", output);
            processUnorderedList(line, input, output, 0);
            return;
        } else if (startsWith(line, ">")) {
            fputs("</p>\n", output);
            processBlockquote(line, input, output);
            return;
        } else {
            processLine(line, output);
        }
    }
    fputs("</p>\n", output);
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

static void processHeading(char* line, FILE* input, FILE* output) {
    size_t level = strspn(line, "#");
    char* title = skipWhitespace(line + level);
    if (level > 6 || title[0] == '\n') {
        processParagraph(line, input, output);
    } else {
        printHeading(title, level, output);
    }
}

static void processFootnote(char* line, FILE* input, FILE* output) {
    char* name = line + 2;
    char* end = strchr(name, ']');
    if (end == NULL || end == name || end[1] != ':') {
        processParagraph(line, input, output);
        return;
    }
    fputs("<p id=\"", output);
    fputr(name, end, output);
    fputs("\">\n", output);
    processLine(skipWhitespace(end + 2), output);
    while (isspace(peek(input)))
        processLine(readLine(input), output);
    fputs("</p>\n", output);
}

static void processFile(FILE* input, FILE* output) {
    char* line = NULL;
    while ((line = readLine(input))) {
        char* start = skipWhitespace(line);
        if (start[0] == '\n') {
        } else if (line[0] == '#') {
            processHeading(line, input, output);
        } else if (line[0] == '>') {
            processBlockquote(line, input, output);
        } else if (startsWith(line, "* ")) {
            processUnorderedList(line, input, output, 0);
        } else if (startsWith(line, "---")) {
            fputs("<hr>\n", output);
        } else if (startsWith(line, "```")) {
            processCodeFence(line, input, output);
        } else if (startsWith(line, "[^")) {
            processFootnote(line, input, output);
        } else {
            int next = peek(input);
            if (next == '=') {
                printHeading(line, 1, output);
                line = readLine(input);
            } else if (next == '-') {
                printHeading(line, 2, output);
                line = readLine(input);
            } else {
                processParagraph(line, input, output);
            }
        }
    }
}

int main(int argc, char* argv[]) {
    processFile(stdin, stdout);
}
