#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "inline.h"

void fputr(char* start, char* end, FILE* output) {
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

char* printEscaped(char* start, char* end, FILE* output) {
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
    return end;
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

static char* processSpan(char* start, char* c, int intraword, int tight,
        int process, char* openTag, char* closeTag, FILE* output) {
    size_t length = strspn(start, c);
    if ((tight && isspace(start[length])) || (!intraword && isalnum(start[-1])))
        return printEscaped(start, start + length, output);
    char* run = strchr(start + length, c[0]);
    for (; run; run = strchr(run + strspn(run, c), c[0])) {
        while (run && run[-1] == '\\')
            run = strchr(run + 1, c[0]);
        if (run && strspn(run, c) == length && !(tight && isspace(run[-1])) &&
                !(!intraword && isalnum(run[length])))
            break;
    }
    if (!run)
        return printEscaped(start, start + length, output);
    fputs(openTag, output);
    if (process)
        processInlines(start + length, run, output);
    else
        printEscaped(start + length, run, output);
    fputs(closeTag, output);
    return run + length;
}

static char* processInlineCode(char* start, FILE* output) {
    return processSpan(start, "`", 1, 0, 0, "<code>", "</code>", output);
}

static char* processInlineMath(char* start, FILE* output) {
    return processSpan(start, "$", 0, 1, 0, "\\(", "\\)", output);
}

static char* processEmphasis(char* start, FILE* output) {
    return processSpan(start, "_", 1, 1, 1, "<em>", "</em>", output);
}

static char* processStrong(char* start, FILE* output) {
    return processSpan(start, "*", 1, 1, 1, "<strong>", "</strong>", output);
}

void processInlines(char* start, char* end, FILE* output) {
    char* p = start;
    while (*p != 0) {
        char* brk = strpbrk(p, "`$*_<![\\");
        if (!brk || (end && brk > end))
            brk = end;
        printEscaped(p, brk, output);
        if (!brk || brk == end)
            return;
        switch (*brk) {
            case '`': p = processInlineCode(brk, output); break;
            case '$': p = processInlineMath(brk, output); break;
            case '*': p = processStrong(brk, output); break;
            case '_': p = processEmphasis(brk, output); break;
            case '<': p = processTag(brk, output); break;
            case '!': p = processImage(brk, output); break;
            case '[': p = processLink(brk, output); break;
            case '\\': p = printBackslash(brk, output); break;
        }
        if (p == brk)
            printEscape(p++, output);
    }
}
