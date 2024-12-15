#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "inline.h"
#include "read.h"
#include "block.h"

static int startsWith(char* string, char* prefix) {
    return string != NULL && strncmp(prefix, string, strlen(prefix)) == 0;
}

static char* skip(char* start, char* characters) {
    return start == NULL ? NULL : start + strspn(start, characters);
}

static char* rskip(char* end, char* characters) {
    for (; strchr(characters, end[-1]); end--);
    return end;
}

static char* rtrim(char* s, char* characters) {
    for (size_t i = strlen(s); i > 0 && strchr(characters, s[i - 1]); i--)
        s[i - 1] = '\0';
    return s;
}

static char* unindent(char* p) {
    return startsWith(p, "    ") ? p + 4 : (p && p[0] == '\t' ? p + 1 : NULL);
}

static int isBlankLine(char* line) {
    char* end = skip(line, " \t");
    return end[0] == '\n' || end[0] == '\r';
}

static void processCodeFence(char* line, FILE* output) {
    size_t length = strspn(line, "`");
    char* language = rtrim(skip(line + length, " \t"), " \t\r\n");
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
    while (unindent(peekLine()))
        printEscaped(unindent(readLine()), NULL, output);
    fputs("</code>\n</pre>\n", output);
}

static void processMathBlock(char* line, FILE* output) {
    line += 2;
    fputs("\\[", output);
    do {
        char* end = strstr(line, "$$");
        if (end && isBlankLine(end + 2)) {
            printEscaped(line, end, output);
            break;
        }
        printEscaped(line, NULL, output);
    } while ((line = readLine()));
    fputs("\\]\n", output);
}

static void processTableRow(char* line, int header, FILE* output) {
    char* p = line + 1;
    fputs("<tr>\n", output);
    for (char* end = p; (end = strchr(end + 1, '|'));) {
        if (end[-1] == '\\')
            continue;
        fputs(header ? "<th>" : "<td>", output);
        processInlines(skip(p, " \t"), rskip(end, " \t"), output);
        fputs(header ? "</th>\n" : "</td>\n", output);
        p = end + 1;
    }
    fputs("</tr>\n", output);
}

static void processTable(char* line, FILE* output) {
    char* nextLine = peekLine();
    int divider = startsWith(nextLine, "|") &&
                  isBlankLine(skip(nextLine, " |-:"));
    fputs("<table>\n", output);
    fputs(divider ? "<thead>\n" : "<tbody>\n", output);
    processTableRow(line, divider, output);
    if (divider) {
        fputs("</thead>\n<tbody>\n", output);
        readLine();
    }
    while (startsWith(peekLine(), "|"))
        processTableRow(readLine(), 0, output);
    fputs("</tbody>\n</table>\n", output);
}

static void processDescriptionItem(char* line, FILE* output) {
    char* key = skip(line + 1, " \t");
    char* separator = strchr(line, ':');
    if (!separator || key[0] == '.')
        return;
    fputs("<dt>\n", output);
    processInlines(key, rskip(separator, " \t"), output);
    fputs("\n</dt>\n<dd>\n", output);
    char* value = rtrim(skip(separator + 1, " \t"), " \t\r\n");
    processInlines(value, NULL, output);
    fputs("\n</dd>\n", output);
}

static void processDescriptionList(char* line, FILE* output) {
    fputs("<dl>\n", output);
    processDescriptionItem(line, output);
    while (startsWith(peekLine(), "= "))
        processDescriptionItem(readLine(), output);
    fputs("</dl>\n", output);
}

static void printHeading(char* title, int level, FILE* output) {
    char openTag[] = "<h0>";
    char closeTag[] = "</h0>\n";
    openTag[2] = '0' + level;
    closeTag[3] = '0' + level;
    fputs(openTag, output);
    processInlines(rtrim(skip(title, " \t"), " \t\r\n"), NULL, output);
    fputs(closeTag, output);
}

static int processHeading(char* line, FILE* output) {
    size_t level = strspn(line, "#");
    if (level == 0 || level > 6 || !isblank(*(line + level)))
        return 0;
    printHeading(rtrim(skip(line + level, " \t"), " \t#\r\n"), level, output);
    return 1;
}

static int processUnderline(char* line, FILE* output) {
    char* next = peekLine();
    if (next && next[0] == '=' && isBlankLine(skip(next, "=")))
        printHeading(line, 1, output);
    else if (next && next[0] == '-' && isBlankLine(skip(next, "-")))
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
    printRaw(name, end, output);
    fputs("\">\n", output);
    processInlines(skip(end + 2, " \t"), NULL, output);
    while (isspace(peek()))  // allows blank lines
        processInlines(skip(readLine(), " \t"), NULL, output);
    fputs("</p>\n", output);
    return 1;
}

static int isParagraphInterrupt(char* line) {
    static char* interrupts[] = {"$$", "```", "---", "* ", "- ", "+ ", "> ",
        ":::", "= ", "| ", "# ", "## ", "### ", "#### ", "##### ", "###### "};
    if (!line || isBlankLine(line))
        return 1;
    for (size_t i = 0; i < sizeof(interrupts)/sizeof(char*); i++)
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

void processBlock(char* line, FILE* output) {
    if (isBlankLine(line))
        return;
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
    else if (startsWith(line, "= "))
        processDescriptionList(line, output);
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
