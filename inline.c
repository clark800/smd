#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "inline.h"

char* skip(char* start, char* characters) {
    return start == NULL ? NULL : start + strspn(start, characters);
}

void fputr(char* start, char* end, FILE* output) {
    if (end)
        fwrite(start, sizeof(char), end - start, output);
    else
        fputs(start, output);
}

static size_t min(size_t a, size_t b) {
    return a > b ? b : a;
}

static int count(char* start, char* end, char c) {
    int count = 0;
    for (char* p = start; p < end; p++)
        if (p[0] == c && p[-1] != '\\')
            count += 1;
    return count;
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
    if (start[1] == '\n' || start[1] == '\r') {
        fputs("<br>\n", output);
        return start + 2;
    } else if (ispunct(start[1])) {
        return printEscape(start + 1, output);
    } else {
        fputc(start[0], output);
        return start + 1;
    }
}

void printEscaped(char* start, char* end, FILE* output) {
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

static char* getNextRun(char* start, char* delimiter) {
    char* endrun = strstr(start, delimiter);
    while (endrun && endrun[-1] == '\\')
        endrun = strstr(endrun + 1, delimiter);
    return endrun;
}

static char* findEnd(char* start, char* delimiter, int intraword, int tight) {
    char* run = start;
    size_t length = strlen(delimiter);
    while ((run = getNextRun(skip(run, delimiter), delimiter))) {
        char* end = skip(run, delimiter) - length;
        if (count(start + length, end, delimiter[0]) % 2 == 0 &&
            !(tight && isspace(run[-1])) &&
            !(!intraword && isalnum(end[length])))
            return end;
    }
    return NULL;
}

static char* processSpan(char* start, char* wrap, int intraword, int tight,
        int process, char* openTags[], char* closeTags[], FILE* output) {
    if (!intraword && isalnum(start[-1]))
        return start;
    size_t runlength = strspn(start, wrap);
    if (tight && isspace(start[runlength]))
        return start;
    size_t length = min(runlength, strlen(wrap));
    char delimiter[8];
    strcpy(delimiter, wrap);
    char* end = NULL;
    for (; length > 0 && end == NULL; length--) {
        delimiter[length] = '\0';
        if ((end = findEnd(start, delimiter, intraword, tight)))
            break;
    }
    if (!end)
        return start;
    fputs(openTags[length-1], output);
    if (process)
        processInlines(start + length, end, output);
    else
        printEscaped(start + length, end, output);
    fputs(closeTags[length-1], output);
    return end + length;
}

static char* processInlineCode(char* start, FILE* output) {
    static char* openTags[] = {"<code>", "<code>", "<code>"};
    static char* closeTags[] = {"</code>", "</code>", "</code>"};
    return processSpan(start, "```", 1, 0, 0, openTags, closeTags, output);
}

static char* processEmphasis(char* start, FILE* output) {
    static char* openTags[] = {"<em>", "<strong>", "<em><strong>"};
    static char* closeTags[] = {"</em>", "</strong>", "</strong></em>"};
    return processSpan(start, "***", 1, 1, 1, openTags, closeTags, output);
}

static char* processInlineMath(char* start, FILE* output) {
    static char* openTags[] = {"\\("};
    static char* closeTags[] = {"\\)"};
    return processSpan(start, "$", 0, 1, 0, openTags, closeTags, output);
}

void processInlines(char* start, char* end, FILE* output) {
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
