#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "read.h"

static char* chomp(char* line) {
    size_t length = strlen(line);
    if (line[length - 1] == '\n')
        line[--length] = '\0';
    if (line[length - 1] == '\r')
        line[--length] = '\0';
    return line;
}

static int isLineEnd(char* s) {
    return s[0] == '\n' || s[0] == '\r';
}

static char* skip(char* start, char* characters) {
    return start == NULL ? NULL : start + strspn(start, characters);
}

static char* trim(char* s, char* characters) {
    for (size_t i = strlen(s); i > 0 && strchr(characters, s[i - 1]); i--)
        s[i - 1] = '\0';
    return s;
}

static char* rskip(char* end, char* characters) {
    int i = 0;
    for (; strchr(characters, end[-1 - i]); i++);
    return end - i;
}

static int startsWith(char* string, char* prefix) {
    return string != NULL && strncmp(prefix, string, strlen(prefix)) == 0;
}

static char* unindent(char* p) {
    return startsWith(p, "    ") ? p + 4 : (p && p[0] == '\t' ? p + 1 : NULL);
}

static int isBlank(char* line) {
    return line == NULL || isLineEnd(skip(line, " \t"));
}

static void fputr(char* start, char* end, FILE* output) {
    if (end)
        fwrite(start, sizeof(char), end - start, output);
    else
        fputs(start, output);
}

static char* printEscape(char* start, FILE* output) {
    switch (start[0]) {
        case '<': fputs("&lt;", output); break;
        case '>': fputs("&gt;", output); break;
        case '&': fputs("&amp;", output); break;
        default: fputc(start[0], output); break;
    }
    return start + 1;
}

static char* printAmpersand(char* start, FILE* output) {
    char* brk = strpbrk(start + 1, "<>&; \t\n");
    if (brk && brk[0] == ';' && brk - start > 1) {
        fputr(start, brk + 1, output);
        return brk + 1;  // HTML Entity
    }
    return printEscape(start, output);
}

static char* printBackslash(char* start, FILE* output) {
    if (isLineEnd(start + 1)) {
        fputs("<br>\n", output);
        return start + 2;
    } else if (ispunct(start[1])) {
        return printEscape(start + 1, output);
    } else {
        fputc(start[0], output);
        return start + 1;
    }
}

static void printEscaped(char* start, char* end, FILE* output) {
    char* brk = NULL;
    for (char* p = start; !end || p < end;) {
        brk = strpbrk(p, "<>&\\");
        if (!brk || (end && brk > end))
            brk = end;
        fputr(p, brk, output);
        if (!brk || brk == end)
            break;
        switch (brk[0]) {
            case '&': p = printAmpersand(brk, output); break;
            case '\\': p = printBackslash(brk, output); break;
            default: p = printEscape(brk, output); break;
        }
    }
}

static char* processTag(char* start, FILE* output) {
    char* content = start + 1;
    if (isspace(content[0]))
        return printEscape(start, output);
    char* end = strchr(content, '>');
    if (end == NULL || end == content)
        return printEscape(start, output);
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
    printEscaped(content, end, output);
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
    if (titleEnd == NULL)
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
    if (hrefEnd == NULL)
        return start;
    fputs("<a href=\"", output);
    fputr(href, hrefEnd, output);
    fputs("\">", output);
    printEscaped(title, titleEnd, output);
    fputs("</a>", output);
    return hrefEnd + 1;
}

static char* processImage(char* start, FILE* output) {
    if (start[1] != '[')
        return start;
    char* title = start + 2;
    char* titleEnd = findClose(start + 1, "[]");
    char* paren = titleEnd + 1;
    if (titleEnd == NULL || paren[0] != '(')
        return start;
    char* href = paren + 1;
    char* hrefEnd = findClose(paren, "()");
    if (hrefEnd == NULL)
        return start;
    fputs("<img src=\"", output);
    fputr(href, hrefEnd, output);
    fputs("\" alt=\"", output);
    fputr(title, titleEnd, output);
    fputs("\">", output);
    return hrefEnd + 1;
}

static char* processWrap(char* start, char* wrap, int intraword, int tight,
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
    while (end && end[-1] == '\\')
        end = strstr(end + 1, delimiter);
    if (end == NULL || (tight && (isspace(content[0]) || isspace(end[-1]))))
        return start;
    if (!intraword && (isalnum(start[-1]) || isalnum(end[length])))
        return start;
    fputs(openTags[length-1], output);
    printEscaped(content, end, output);
    fputs(closeTags[length-1], output);
    return end + length;
}

static char* processInlineCode(char* start, FILE* output) {
    static char* openTags[] = {"<code>", "<code>", "<code>"};
    static char* closeTags[] = {"</code>", "</code>", "</code>"};
    return processWrap(start, "```", 1, 0, openTags, closeTags, output);
}

static char* processEmphasis(char* start, FILE* output) {
    static char* openTags[] = {"<em>", "<strong>", "<em><strong>"};
    static char* closeTags[] = {"</em>", "</strong>", "</strong></em>"};
    return processWrap(start, "***", 1, 1, openTags, closeTags, output);
}

static char* processInlineMath(char* start, FILE* output) {
    static char* openTags[] = {"\\("};
    static char* closeTags[] = {"\\)"};
    return processWrap(start, "$", 0, 1, openTags, closeTags, output);
}

static void processInlines(char* start, char* end, FILE* output) {
    char* p = start;
    while (*p != 0) {
        char* brk = strpbrk(p, "`$*<![\\");
        if (!brk || (end && brk > end))
            brk = end;
        printEscaped(p, brk, output);
        if (!brk || brk == end)
            return;
        switch (*brk) {
            case '`': p = processInlineCode(brk, output); break;
            case '$': p = processInlineMath(brk, output); break;
            case '*': p = processEmphasis(brk, output); break;
            case '<': p = processTag(brk, output); break;
            case '!': p = processImage(brk, output); break;
            case '[': p = processLink(brk, output); break;
            case '\\': p = printBackslash(brk, output); break;
        }
        if (p == brk)
            fputc(*p++, output);
    }
}

static void processCodeFence(char* line, FILE* output) {
    size_t length = strspn(line, "`");
    char* language = chomp(skip(line + length, " \t"));
    if (language[0] != '\0') {
        fputs("<pre>\n<code class=\"language-", output);
        fputs(language, output);
        fputs("\">\n", output);
    } else {
        fputs("<pre>\n<code>\n", output);
    }
    while ((line = readLine()) && strspn(line, "`") < length)
        printEscaped(line, NULL, output);
    fputs("</code>\n</pre>\n", output);
}

static void processCodeBlock(char* line, FILE* output) {
    fputs("<pre>\n<code>\n", output);
    printEscaped(unindent(line), NULL, output);
    while ((line = unindent(readLine())))
        printEscaped(line, NULL, output);
    fputs("</code>\n</pre>\n", output);
}

static void processMathBlock(char* line, FILE* output) {
    line += 2;
    fputs("\\[", output);
    do {
        char* end = strstr(line, "$$");
        if (end && isLineEnd(skip(end + 2, " \t"))) {
            printEscaped(line, end, output);
            break;
        }
        printEscaped(line, NULL, output);
    } while ((line = readLine()));
    fputs("\\]\n", output);
}

static void processTableRow(char* line, int header, FILE* output) {
    char* p = line + 1;
    if (isLineEnd(skip(p, " \t|-:")))
        return;  // ignore divider row
    fputs("<tr>", output);
    for (char* end = p; (end = strchr(end + 1, '|'));) {
        if (end[-1] == '\\')
            continue;
        fputs(header ? "<th>" : "<td>", output);
        processInlines(skip(p, " \t"), rskip(end, " \t"), output);
        fputs(header ? "</th>" : "</td>", output);
        p = end + 1;
    }
    fputs("</tr>\n", output);
}

static void processTable(char* line, FILE* output) {
    fputs("<table>\n<thead>\n", output);
    processTableRow(line, 1, output);
    fputs("</thead>\n<tbody>\n", output);
    while (startsWith(peekLine(), "|"))
        processTableRow(readLine(), 0, output);
    fputs("</tbody>\n</table>\n", output);
}

static void printHeading(char* title, int level, FILE* output) {
    char openTag[] = "<h0>";
    char closeTag[] = "</h0>\n";
    openTag[2] = '0' + level;
    closeTag[3] = '0' + level;
    fputs(openTag, output);
    processInlines(trim(chomp(skip(title, " \t")), " \t"), NULL, output);
    fputs(closeTag, output);
}

static int processHeading(char* line, FILE* output) {
    size_t level = strspn(line, "#");
    if (level == 0 || level > 6 || !isblank(*(line + level)))
        return 0;
    printHeading(trim(chomp(skip(line + level, " \t")), " \t#"), level, output);
    return 1;
}

int processUnderline(char* line, FILE* output) {
    char* next = peekLine();
    if (next && next[0] == '=' && isLineEnd(skip(skip(next, "="), " \t")))
        printHeading(line, 1, output);
    else if (next && next[0] == '-' && isLineEnd(skip(skip(next, "-"), " \t")))
        printHeading(line, 2, output);
    else return 0;
    readLine();
    return 1;
}

static int processFootnote(char* line, FILE* output) {
    if (!startsWith(line, "[^"))
        return 0;
    char* name = line + 2;
    char* end = strchr(name, ']');
    if (end == NULL || end == name || end[1] != ':')
        return 0;
    fputs("<p id=\"", output);
    fputr(name, end, output);
    fputs("\">\n", output);
    processInlines(skip(end + 2, " \t"), NULL, output);
    while (isspace(peek()))  // allows blank lines
        processInlines(skip(readLine(), " \t"), NULL, output);
    fputs("</p>\n", output);
    return 1;
}

static int isParagraphInterrupt(char* line) {
    static char* interrupts[] = {"$$", "```", "---", "* ", "- ", "+ ", ">",
        "| ", "# ", "## ", "### ", "#### ", "##### ", "###### "};
    if (isBlank(line))
        return 1;
    for (int i = 0; i < sizeof(interrupts)/sizeof(char*); i++)
        if (startsWith(line, interrupts[i]))
            return 1;
    if (isdigit(line[0]) && line[1] == '.' && line[2] == ' ')
        return 1;
    return 0;
}

static void processParagraph(char* line, FILE* output) {
    fputs("<p>\n", output);
    processInlines(line, NULL, output);
    while (!isParagraphInterrupt(peekLine()))
        processInlines(readLine(), NULL, output);
    fputs("</p>\n", output);
}

static void processBlock(char* line, FILE* output) {
    if (startsWith(line, "---"))
        fputs("<hr>\n", output);
    else if (unindent(line))
        processCodeBlock(line, output);
    else if (startsWith(line, "```"))
        processCodeFence(line, output);
    else if (startsWith(line, "$$"))
        processMathBlock(line, output);
    else if (startsWith(line, "| "))
        processTable(line, output);
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
