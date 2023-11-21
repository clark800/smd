int isLineEnd(char* s);
char* skip(char* start, char* characters);
void fputr(char* start, char* end, FILE* output);
void printEscaped(char* start, char* end, FILE* output);
void processInlines(char* start, char* end, FILE* output);
