#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "inline.h"
#include "read.h"

static char* trim(char* s, char* characters) {
    for (size_t i = strlen(s); i > 0 && strchr(characters, s[i - 1]); i--)
        s[i - 1] = '\0';
    return s;
}

static char* chomp(char* line) {
    size_t length = strlen(line);
    if (line[length - 1] == '\n')
        line[--length] = '\0';
    if (line[length - 1] == '\r')
        line[--length] = '\0';
    return line;
}

static int isBlank(char* line) {
    return line == NULL || isLineEnd(skip(line, " \t"));
}

static int startsWith(char* string, char* prefix) {
    return string != NULL && strncmp(prefix, string, strlen(prefix)) == 0;
}

static char* unindent(char* p) {
    return startsWith(p, "    ") ? p + 4 : (p && p[0] == '\t' ? p + 1 : NULL);
}

static char* rskip(char* end, char* characters) {
    int i = 0;
    for (; strchr(characters, end[-1 - i]); i++);
    return end - i;
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

int main(void) {
    char* line = NULL;
    initContext(stdin, stdout);
    while ((line = beginBlock()))
        if (!isBlank(line))
            processBlock(line, stdout);
}
