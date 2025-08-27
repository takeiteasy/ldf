#define LD_IMPLEMENTATION
#include "ldzero.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path, size_t *size) {
    char *result = NULL;
    FILE *file = fopen(path, "rb");
    if (!file)
        goto BAIL;
    fseek(file, 0, SEEK_END);
    size_t _size = ftell(file);
    if (size)
        *size = _size;
    fseek(file, 0, SEEK_SET);
    if (!(result = malloc(_size + 1)))
        goto BAIL;
    fread(result, 1, _size, file);
    result[_size] = '\0';
BAIL:
    if (file)
        fclose(file);
    return result;
}

static const char* token_type_to_string(enum ld_token_type type) {
    switch (type) {
        case LD_SEXP:
            return "SEXP";
        case LD_ARRAY:
            return "ARRAY";
        case LD_STRING:
            return "STRING";
        case LD_PRIMITIVE:
            return "PRIMITIVE";
        default:
            return "UNDEFINED";
    }
}

static void print_token(const char *input, struct ld_token *token) {
    int len = token->end - token->start;
    if (len > 0)
        printf("%.*s", len, input + token->start);
}

int main(int argc, const char *argv[]) {
    struct ld_parser parser;
    struct ld_token tokens[256];
    int token_count;

    int input_len = 0;
    const char *input = read_file("test.ldf", (size_t*)&input_len);
    if (!input) {
        printf("Failed to read input file\n");
        return 1;
    }

    ld_init(&parser);
    if ((token_count = ld_parse(&parser, input, input_len, tokens, 256)) < 0) {
        printf("Parse error: %d\n", token_count);
        return 1;
    }
    
    printf("Successfully parsed %d tokens:\n\n", token_count);
    for (int i = 0; i < token_count; i++) {
        printf("Token %d: Type=%s, Start=%d, End=%d, Size=%d, Parent=%d\n", 
               i, 
               token_type_to_string(tokens[i].type),
               tokens[i].start,
               tokens[i].end,
               tokens[i].size,
               tokens[i].parent);
        printf("  Content: \"");
        print_token(input, &tokens[i]);
        printf("\"\n\n");
    }
    return 0;
}