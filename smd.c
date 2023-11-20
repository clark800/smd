#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "read.h"

static char* chomp(char* line) {
    size_t length = strlen(line);
    if (line[length - 1] == '\n')
        line[length - 1] = '\0';
    return line;
}

static char* skip(char* line, char* characters) {
    return line + strspn(line, characters);
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
    char* atsign = memchr(content, '@', end - content);
    if (space != NULL || (colon == NULL && atsign == NULL)) {
        // assume this is an HTML tag
        fputr(start, end + 1, output);
        return end + 1;
    }
    fputs("<a href=\"", output);
    if (colon == NULL && atsign != NULL)
        fputs("mailto:", output);
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
    char* titleEnd = findClose(start, "[]");
    if (titleEnd == NULL || titleEnd == title)
        return start;
    if (title[0] == '^') {
        fputs("<sup><a href=\"#", output);
        fputr(title + 1, titleEnd, output);
        fputs("\">*</a></sup>", output);
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
    fputs("\" alt=\"", output);
    fputr(title, titleEnd, output);
    fputs("\">", output);
    return hrefEnd + 1;
}

static char* processBackslash(char* start, FILE* output) {
    char* p = start;
    if (start[1] == '\n') {
        fputs("<br>\n", output);
        return start + 2;
    }
    fputc(*p++, output);
    if (*p != 0)
        fputc(*p++, output);
    return p;
}

static char* processVerbatim(char* start, char* end, FILE* output) {
    char* brk = NULL;
    for (char* p = start; p < end; p = brk + 1) {
        brk = strpbrk(p, "<>&");
        if (brk == NULL || brk > end) {
            fputr(p, end, output);
            break;
        }
        fputr(p, brk, output);
        switch (brk[0]) {
            case '<': fputs("&lt;", output); break;
            case '>': fputs("&gt;", output); break;
            case '&': fputs("&amp;", output); break;
        }
    }
    return end;
}

static char* processWrap(char* start, char* wrap, int tightbits,
        char* openTags[], char* closeTags[], FILE* output) {
    size_t maxlen = strlen(wrap);
    char search[] = {wrap[0], '\0'};
    size_t length = strspn(start, search);
    char delimiter[16];
    if (length > maxlen || length >= sizeof(delimiter)) {
        fputr(start, start + length, output);
        return start + length;
    }
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

static char* processInlineMath(char* start, FILE* output) {
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
            case '$': p = processInlineMath(brk, output); break;
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

static void processCodeFence(char* line, FILE* output) {
    char delimiter[] = "````````````````";
    size_t length = strspn(line, "`");
    if (length < sizeof(delimiter))
        delimiter[length] = '\0';
    char* language = chomp(skip(line + length, " \t"));
    if (language[0] != '\0') {
        fputs("<pre>\n<code class=\"language-", output);
        fputs(language, output);
        fputs("\">\n", output);
    } else {
        fputs("<pre>\n<code>\n", output);
    }
    while ((line = readLine()) && !startsWith(line, delimiter))
        fputs(line, output);
    fputs("</code>\n</pre>\n", output);
}

static void processMath(char* line, FILE* output) {
    line += 2;
    fputs("\\[", output);
    do {
        char* end = strstr(line, "$$");
        if (end && skip(end + 2, " \t")[0] == '\n') {
            processVerbatim(line, end, output);
            break;
        }
        processVerbatim(line, line + strlen(line), output);
    } while ((line = readLine()));
    fputs("\\]\n", output);
}

static int isParagraphInterrupt(char* line) {
    char* interrupts[] = {"$$", "```", "---", "* ", "- ", "+ ", ">",
        "# ", "## ", "### ", "#### ", "##### ", "###### "};
    if (line == NULL)
        return 1;
    for (int i = 0; i < sizeof(interrupts)/sizeof(char*); i++)
        if (startsWith(line, interrupts[i]))
            return 1;
    return 0;
}

static void processParagraph(char* line, FILE* output) {
    fputs("<p>\n", output);
    for (; !isBlank(line); line = readLine()) {
        processLine(line, output);
        if (isParagraphInterrupt(peekLine()))
            break;
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

static int processHeading(char* line, FILE* output) {
    size_t level = strspn(line, "#");
    if (level > 6 || !isblank(*(line + level)))
        return 0;
    printHeading(skip(line + level, " \t"), level, output);
    return 1;
}

static int processFootnote(char* line, FILE* output) {
    char* name = line + 2;
    char* end = strchr(name, ']');
    if (end == NULL || end == name || end[1] != ':')
        return 0;
    fputs("<p id=\"", output);
    fputr(name, end, output);
    fputs("\">\n", output);
    processLine(skip(end + 2, " \t"), output);
    while (isspace(peek()))  // allows blank lines
        processLine(readLine(), output);
    fputs("</p>\n", output);
    return 1;
}

int processUnderline(char* line, FILE* output) {
    char* next = peekLine();
    char level = 0;
    if (next && next[0] == '=' && skip(skip(next, "="), " \t")[0] == '\n')
        level = 1;
    if (next && next[0] == '-' && skip(skip(next, "-"), " \t")[0] == '\n')
        level = 2;
    if (level == 0)
        return 0;
    printHeading(line, level, output);
    readLine();
    return 1;
}

static void processBlock(char* line, FILE* output) {
    if (startsWith(line, "---"))
        fputs("<hr>\n", output);
    else if (startsWith(line, "```"))
        processCodeFence(line, output);
    else if (startsWith(line, "$$"))
        processMath(line, output);
    else {
        if (processHeading(line, output))
            return;
        if (processUnderline(line, output))
            return;
        if (processFootnote(line, output))
            return;
        processParagraph(line, output);
    }
}

int main(int argc, char* argv[]) {
    char* line = NULL;
    initContext(stdin, stdout);
    while ((line = beginBlock()))
        if (!isBlank(line))
            processBlock(line, stdout);
}
