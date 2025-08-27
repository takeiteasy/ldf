/* ldf -- https://github.com/takeiteasy/ldf

 Copyright (C) 2025 George Watson

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <https://www.gnu.org/licenses/>. */

#ifndef LD_HEADER
#define LD_HEADER
#ifdef __cplusplus
extern "C" {
#endif

struct ld_parser {
    unsigned int pos;     /* offset in the SEDF string */
    unsigned int toknext; /* next token to allocate */
    int toksuper;         /* superior token node, e.g. parent object or array */
};

enum ld_token_type {
    LD_UNDEFINED = 0,
    LD_SEXP = 1 << 0,     /* S-expression: (...) */
    LD_ARRAY = 1 << 1,    /* Array: #(...) */
    LD_STRING = 1 << 2,   /* String: "..." */
    LD_PRIMITIVE = 1 << 3 /* Primitive: :keyword, number, symbol */
};

enum ld_err {
    /* Not enough tokens were provided */
    LD_ERROR_NOMEM = -1,
    /* Invalid character */
    LD_ERROR_INVAL = -2,
    /* The data is not complete, more bytes expected */
    LD_ERROR_PART = -3
};

struct ld_token {
    enum ld_token_type type;
    int start;
    int end;
    int size;
    int parent;
};

void ld_init(struct ld_parser *parser);
int ld_parse(struct ld_parser *parser, const char *input, const int input_length, struct ld_token *tokens, const unsigned int max_tokens);

#if __cplusplus
}
#endif
#endif // LD_HEADER

#ifdef LD_IMPLEMENTATION
#ifndef NULL
#define NULL (void*)0
#endif

/* Allocates a fresh unused token from the token pool. */
static struct ld_token *ld_alloc_token(struct ld_parser *parser, 
                                           struct ld_token *tokens,
                                           const int num_tokens) {
    struct ld_token *tok;
    if (parser->toknext >= num_tokens)
        return NULL;
    tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1;
    tok->size = 0;
    tok->parent = -1;
    return tok;
}

/* Fills token type and boundaries. */
static void ld_fill_token(struct ld_token *token, const enum ld_token_type type,
                            const int start, const int end) {
    token->type = type;
    token->start = start;
    token->end = end;
    token->size = 0;
}

/* Fills next available token with primitive. */
static int ld_parse_primitive(struct ld_parser *parser, const char *input,
                                const int len, struct ld_token *tokens,
                                const int num_tokens) {
    struct ld_token *token;
    int start = parser->pos;
    for (; parser->pos < len && input[parser->pos] != '\0'; parser->pos++) {
        switch (input[parser->pos]) {
        case '\t':
        case '\r':
        case '\n':
        case ' ':
        case '(':
        case ')':
        case ';':
            goto found;
        default:
            break;
        }
        if (input[parser->pos] < 32 || input[parser->pos] >= 127) {
            parser->pos = start;
            return LD_ERROR_INVAL;
        }
    }

found:
    if (tokens == NULL) {
        parser->pos--;
        return 0;
    }
    if ((token = ld_alloc_token(parser, tokens, num_tokens)) == NULL) {
        parser->pos = start;
        return LD_ERROR_NOMEM;
    }
    ld_fill_token(token, LD_PRIMITIVE, start, parser->pos);
    token->parent = parser->toksuper;
    parser->pos--;
    return 0;
}

/* Fills next token with SEDF string. */
static int ld_parse_string(struct ld_parser *parser, const char *input,
                             const int len, struct ld_token *tokens,
                             const int num_tokens) {
    struct ld_token *token;
    /* Skip starting quote */
    int start = parser->pos++;

    for (; parser->pos < len && input[parser->pos] != '\0'; parser->pos++) {
        char c = input[parser->pos];

        /* end of string */
        if (c == '\"') {
            if (tokens == NULL)
                return 0;
            token = ld_alloc_token(parser, tokens, num_tokens);
            if (token == NULL) {
                parser->pos = start;
                return LD_ERROR_NOMEM;
            }
            ld_fill_token(token, LD_STRING, start + 1, parser->pos);
            token->parent = parser->toksuper;
            return 0;
        }

        if (c == '\\' && parser->pos + 1 < len) {
            parser->pos++;
            switch (input[parser->pos]) {
            case '\"':
            case '\\':
            case '/':
            case 'b':
            case 'f':
            case 'r':
            case 'n':
            case 't':
                break;
            /* escaped symbol \xXX */
            case 'x':
                parser->pos++;
                for (int i = 0; i < 2 && parser->pos < len && input[parser->pos] != '\0'; i++) {
                    /* If it isn't a hex character we have an error */
                    if (!((input[parser->pos] >= 48 && input[parser->pos] <= 57) ||   /* 0-9 */
                          (input[parser->pos] >= 65 && input[parser->pos] <= 70) ||   /* A-F */
                          (input[parser->pos] >= 97 && input[parser->pos] <= 102))) { /* a-f */
                        parser->pos = start;
                        return LD_ERROR_INVAL;
                    }
                    parser->pos++;
                }
                parser->pos--;
                break;
            /* escaped symbol \uXXXX */
            case 'u':
                parser->pos++;
                for (int i = 0; i < 4 && parser->pos < len && input[parser->pos] != '\0'; i++) {
                    /* If it isn't a hex character we have an error */
                    if (!((input[parser->pos] >= 48 && input[parser->pos] <= 57) ||   /* 0-9 */
                          (input[parser->pos] >= 65 && input[parser->pos] <= 70) ||   /* A-F */
                          (input[parser->pos] >= 97 && input[parser->pos] <= 102))) { /* a-f */
                        parser->pos = start;
                        return LD_ERROR_INVAL;
                    }
                    parser->pos++;
                }
                parser->pos--;
                break;
            /* Unexpected symbol */
            default:
                parser->pos = start;
                return LD_ERROR_INVAL;
            }
        }
    }
    parser->pos = start;
    return LD_ERROR_PART;
}

void ld_init(struct ld_parser *parser) {
    parser->pos = 0;
    parser->toknext = 0;
    parser->toksuper = -1;
}

int ld_parse(struct ld_parser *parser, const char *input, const int input_length, 
               struct ld_token *tokens, const unsigned int max_tokens) {
    int r, i;
    struct ld_token *token;
    int count = parser->toknext;

    for (; parser->pos < input_length && input[parser->pos] != '\0'; parser->pos++) {
        char c;
        enum ld_token_type type;
        switch ((c = input[parser->pos])) {
        case '(':
            count++;
            if (tokens == NULL)
                break;
            if ((token = ld_alloc_token(parser, tokens, max_tokens)) == NULL)
                return LD_ERROR_NOMEM;
            if (parser->toksuper != -1) {
                struct ld_token *t = &tokens[parser->toksuper];
                t->size++;
                token->parent = parser->toksuper;
            }
            token->type = LD_SEXP;
            token->start = parser->pos;
            parser->toksuper = parser->toknext - 1;
            break;
        case ')':
            if (tokens == NULL)
                break;
            for (i = parser->toknext - 1; i >= 0; i--) {
                token = &tokens[i];
                if (token->start != -1 && token->end == -1) {
                    if (token->type != LD_SEXP && token->type != LD_ARRAY) {
                        return LD_ERROR_INVAL;
                    }
                    parser->toksuper = -1;
                    token->end = parser->pos + 1;
                    break;
                }
            }
            /* Error if unmatched closing bracket */
            if (i == -1)
                return LD_ERROR_INVAL;
            for (; i >= 0; i--) {
                token = &tokens[i];
                if (token->start != -1 && token->end == -1) {
                    parser->toksuper = i;
                    break;
                }
            }
            break;
        case '#':
            /* Look ahead for array syntax #(...) */
            if (parser->pos + 1 < input_length && input[parser->pos + 1] == '(') {
                count++;
                if (tokens == NULL) {
                    parser->pos++; /* Skip the '(' */
                    break;
                }
                if ((token = ld_alloc_token(parser, tokens, max_tokens)) == NULL) 
                    return LD_ERROR_NOMEM;
                
                if (parser->toksuper != -1) {
                    struct ld_token *t = &tokens[parser->toksuper];
                    t->size++;
                    token->parent = parser->toksuper;
                }
                token->type = LD_ARRAY;
                token->start = parser->pos; /* Start at '#' */
                parser->toksuper = parser->toknext - 1;
                parser->pos++; /* Skip the '(' - the main loop will increment pos again */
            } else {
                /* Treat as primitive if not followed by '(' */
                if ((r = ld_parse_primitive(parser, input, input_length, tokens, max_tokens)) < 0)
                    return r;
                count++;
                if (parser->toksuper != -1 && tokens != NULL)
                    tokens[parser->toksuper].size++;
            }
            break;
        case '\"':
            if ((r = ld_parse_string(parser, input, input_length, tokens, max_tokens)) < 0)
                return r;
            count++;
            if (parser->toksuper != -1 && tokens != NULL)
                tokens[parser->toksuper].size++;
            break;
        case '\t':
        case '\r':
        case '\n':
        case ' ':
            break;
        case ';': // skip comments and advance to end of line 
            while (parser->pos < input_length && 
                   input[parser->pos] != '\0' && 
                   input[parser->pos] != '\n' && 
                   input[parser->pos] != '\r')
                parser->pos++;
            break;
        /* Everything else is a primitive (keywords, numbers, symbols) */
        default:
            if ((r = ld_parse_primitive(parser, input, input_length, tokens, max_tokens)) < 0)
                return r;
            count++;
            if (parser->toksuper != -1 && tokens != NULL)
                tokens[parser->toksuper].size++;
            break;
        }
    }

    /* Unmatched opened object or array */
    if (tokens != NULL)
        for (i = parser->toknext - 1; i >= 0; i--)
            if (tokens[i].start != -1 && tokens[i].end == -1)
                return LD_ERROR_PART;
    return count;
}
#endif // LD_IMPLEMENTATION
