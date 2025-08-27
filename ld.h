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

#include <stdlib.h>
#include <stddef.h>
#include <string.h>

struct ld_parser {
    const char *source;
    size_t length;
    size_t position;
};

struct ld_pair {
    const char *key;
    struct ld_atom *value;
};

struct ld_object {
    struct ld_pair *pairs;
    size_t count;
};

struct ld_array {
    struct ld_atom **items;
    size_t count;
};

struct ld_atom {
    enum {
        LD_ATOM_NULL,
        LD_ATOM_OBJECT,
        LD_ATOM_ARRAY,
        LD_ATOM_STRING,
        LD_ATOM_NUMBER,
        LD_ATOM_BOOLEAN,
        LD_ATOM_SYMBOL
    } type;
    union {
        struct ld_object *object;
        struct ld_array *array;
        char *string;
        double number;
        int boolean;
        char *symbol;
    } value;
};

enum ld_error_type {
    LD_OK = 0,
    LD_OUT_OF_MEMORY = -1,
    LD_UNEXPECTED_CHAR = -2,
    LD_UNEXPECTED_EOF = -3,
    LD_UNEXPECTED_TOKEN = -4,
    LD_UNKNOWN_ERROR = -5
};

void ld_init(struct ld_parser *parser, const char *source, size_t length);
enum ld_error_type ld_parse(struct ld_parser *parser, struct ld_atom **out, size_t *count);
void ld_free(struct ld_atom *atom);

#if __cplusplus
}
#endif
#endif // LD_HEADER

#ifdef LD_IMPLEMENTATION
enum ld_token_type {
    LD_TOKEN_SYMBOL = 0,
    LD_TOKEN_OBJECT = 1 << 0,
    LD_TOKEN_ARRAY = 1 << 1,
    LD_TOKEN_STRING = 1 << 2,
    LD_TOKEN_PRIMITIVE = 1 << 3,
    LD_TOKEN_OBJECT_CLOSE = 1 << 4,
};

struct ld_token {
    enum ld_token_type type;
    const char *start;
    size_t length;
};

static int is_eof(struct ld_parser *parser) {
    return parser->position >= parser->length;
}

static void consume(struct ld_parser *parser) {
    parser->position++;
}

static enum ld_error_type skip_whitespace(struct ld_parser *parser) {
    while (!is_eof(parser))
        switch (parser->source[parser->position]) {
            case ' ':
            case '\t':
            case '\n':
            case '\r':
                consume(parser);
                break;
            default:
                return LD_OK;
        }
    return LD_OK;
}

static char cursor(struct ld_parser *parser) {
    return is_eof(parser) ? '\0' : parser->source[parser->position];
}

static char* cursor_ptr(struct ld_parser *parser) {
    return is_eof(parser) ? NULL : (char*)&parser->source[parser->position];
}

static char peek(struct ld_parser *parser) {
    return is_eof(parser) || parser->position + 1 >= parser->length ? '\0' : parser->source[parser->position + 1];
}

static int is_valid_lisp_symbol_char(char c) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
        return 1;
    if (c >= '0' && c <= '9')
        return 1;
    switch (c) {
        case '-':
        case '+':
        case '*':
        case '/':
        case '@':
        case '$':
        case '%':
        case '&':
        case '=':
        case '<':
        case '>':
        case '.':
        case '_':
        case '~':
        case '?':
        case '!':
        case '[':
        case ']':
        case '{':
        case '}':
        case '^':
        case ':':
            return 1;
        default:
            return 0;
    }
}

void create_token(struct ld_token *token, enum ld_token_type type, const char *start, size_t length) {
    token->type = type;
    token->start = start;
    token->length = length;
}

enum ld_error_type ld_next(struct ld_parser *parser, struct ld_token *out) {
    enum ld_error_type err = LD_OK;
    if ((err = skip_whitespace(parser)) != LD_OK)
        goto BAIL;
    char c = cursor(parser);
    switch (c) {
        case ';': // skip comments and advance to the next line
            while (!is_eof(parser) && cursor(parser) != '\n' && cursor(parser) != '\r')
                consume(parser);
            return ld_next(parser, out);
        case '(':
            create_token(out, LD_TOKEN_OBJECT, cursor_ptr(parser), 1);
            consume(parser);
            break;
        case ')':
            create_token(out, LD_TOKEN_OBJECT_CLOSE, cursor_ptr(parser), 1);
            consume(parser);
            break;
        case '"':
            create_token(out, LD_TOKEN_STRING, cursor_ptr(parser), 1);
            consume(parser);
            break;
        case '#':
            create_token(out, LD_TOKEN_ARRAY, cursor_ptr(parser), 1);
            consume(parser);
            break;
        default:;
            if (!is_valid_lisp_symbol_char(c)) {
                err = LD_UNEXPECTED_CHAR;
                goto BAIL;
            }

            enum ld_token_type token_type = (c == ':') ? LD_TOKEN_SYMBOL : LD_TOKEN_PRIMITIVE;
            create_token(out, token_type, cursor_ptr(parser), 0);
            size_t start = parser->position;
            while (is_valid_lisp_symbol_char(c)) {
                consume(parser);
                if (is_eof(parser))
                    goto FOUND;
                else {
                    switch ((c = cursor(parser))) {
                        case ' ':
                        case '\t':
                        case '\n':
                        case '\r':
                        case ')':
                            goto FOUND;
                        default:
                            break;
                    }
                };
            }
        FOUND:
            if (start == parser->position) {
                err = LD_UNEXPECTED_EOF;
                goto BAIL;
            } else {
                out->start = parser->source + start;
                out->length = parser->position - start;
            }
            break;
    }
BAIL:
    return err;
}

static struct ld_atom* ld_create_atom(void) {
    struct ld_atom *atom = (struct ld_atom*)malloc(sizeof(struct ld_atom));
    if (atom)
        memset(atom, 0, sizeof(struct ld_atom));
    return atom;
}

static char* ld_create_string(const char *src, size_t length) {
    char *str = (char*)malloc(length + 1);
    if (str) {
        memcpy(str, src, length);
        str[length] = '\0';
    }
    return str;
}

static enum ld_error_type ld_parse_atom(struct ld_parser *parser, struct ld_atom *atom);

static enum ld_error_type ld_parse_object(struct ld_parser *parser, struct ld_atom *atom) {
    atom->type = LD_ATOM_OBJECT;
    atom->value.object = (struct ld_object*)malloc(sizeof(struct ld_object));
    if (!atom->value.object)
        return LD_OUT_OF_MEMORY;
    atom->value.object->pairs = NULL;
    atom->value.object->count = 0;
    
    struct ld_token token;
    enum ld_error_type err = LD_OK;
    size_t capacity = 8;
    struct ld_pair *pairs = (struct ld_pair*)malloc(capacity * sizeof(struct ld_pair));
    if (!pairs)
        return LD_OUT_OF_MEMORY;

    while (1) {
        if ((err = skip_whitespace(parser)) != LD_OK)
            goto BAIL;
        if (cursor(parser) == ')') {
            consume(parser);
            break;
        }
        if ((err = ld_next(parser, &token)) != LD_OK)
            goto BAIL;
        if (token.type != LD_TOKEN_SYMBOL) {
            err = LD_UNEXPECTED_TOKEN;
            goto BAIL;
        }

        if (atom->value.object->count >= capacity) {
            capacity *= 2;
            struct ld_pair *new_pairs = (struct ld_pair*)realloc(pairs, capacity * sizeof(struct ld_pair));
            if (!new_pairs) {
                err = LD_OUT_OF_MEMORY;
                goto BAIL;
            }
            pairs = new_pairs;
        }
        
        pairs[atom->value.object->count].key = ld_create_string(token.start, token.length);
        if (!pairs[atom->value.object->count].key) {
            err = LD_OUT_OF_MEMORY;
            goto BAIL;
        }
        
        struct ld_atom *value_atom = ld_create_atom();
        if (!value_atom) {
            err = LD_OUT_OF_MEMORY;
            goto BAIL;
        }
        
        if ((err = ld_parse_atom(parser, value_atom)) != LD_OK) {
            ld_free(value_atom);
            goto BAIL;
        }
        
        pairs[atom->value.object->count].value = value_atom;
        atom->value.object->count++;
    }
    atom->value.object->pairs = pairs;

BAIL:
    if (pairs && err != LD_OK) {
        for (size_t i = 0; i < atom->value.object->count; i++) {
            free((void*)pairs[i].key);
            if (pairs[i].value)
                ld_free(pairs[i].value);
        }
        free(pairs);
    }
    return err;
}

static enum ld_error_type ld_parse_array(struct ld_parser *parser, struct ld_atom *atom) {
    size_t capacity = 8;
    struct ld_atom **items = NULL;
    enum ld_error_type err = LD_OK;

    atom->type = LD_ATOM_ARRAY;
    atom->value.array = (struct ld_array*)malloc(sizeof(struct ld_array));
    if (!atom->value.array) {
        err = LD_OUT_OF_MEMORY;
        goto BAIL;
    }
    atom->value.array->items = NULL;
    atom->value.array->count = 0;
    
    struct ld_token token;
    if ((err = ld_next(parser, &token)) != LD_OK)
        goto BAIL;
    if (token.type != LD_TOKEN_OBJECT) {
        err = LD_UNEXPECTED_TOKEN;
        goto BAIL;
    }

    if (!(items = (struct ld_atom**)malloc(capacity * sizeof(struct ld_atom*)))) {
        err = LD_OUT_OF_MEMORY;
        goto BAIL;
    }
    
    while (1) {
        if ((err = skip_whitespace(parser)) != LD_OK)
            goto BAIL;
        
        if (cursor(parser) == ')') {
            consume(parser);
            break;
        }
        
        if (atom->value.array->count >= capacity) {
            capacity *= 2;
            struct ld_atom **new_items = (struct ld_atom**)realloc(items, capacity * sizeof(struct ld_atom*));
            if (!new_items) {
                err = LD_OUT_OF_MEMORY;
                goto BAIL;
            }
            items = new_items;
        }
        
        struct ld_atom *item_atom = ld_create_atom();
        if (!item_atom) {
            err = LD_OUT_OF_MEMORY;
            goto BAIL;
        }
        
        if ((err = ld_parse_atom(parser, item_atom)) != LD_OK) {
            ld_free(item_atom);
            goto BAIL;
        }
        
        items[atom->value.array->count] = item_atom;
        atom->value.array->count++;
    }
    
    atom->value.array->items = items;
    return LD_OK;
    
BAIL:
    if (items) {
        for (size_t i = 0; i < atom->value.array->count; i++) {
            if (items[i])
                ld_free(items[i]);
        }
        free(items);
    }
    return err;
}

static enum ld_error_type ld_parse_string(struct ld_parser *parser, struct ld_atom *atom, struct ld_token *token) {
    atom->type = LD_ATOM_STRING;
    size_t start_pos = parser->position;
    while (!is_eof(parser) && cursor(parser) != '"')
        consume(parser);
    if (is_eof(parser))
        return LD_UNEXPECTED_EOF;

    size_t length = parser->position - start_pos;
    atom->value.string = ld_create_string(parser->source + start_pos, length);
    if (!atom->value.string)
        return LD_OUT_OF_MEMORY;
    consume(parser);
    return LD_OK;
}

static enum ld_error_type ld_parse_primitive(struct ld_parser *parser, struct ld_atom *atom, struct ld_token *token) {
    if (token->length == 1 && *token->start == 't') {
        atom->type = LD_ATOM_BOOLEAN;
        atom->value.boolean = 1;
        return LD_OK;
    }
    if (token->length == 3 && strncmp(token->start, "nil", 3) == 0) {
        atom->type = LD_ATOM_NULL;
        return LD_OK;
    }
    
    char *endptr;
    double num = strtod(token->start, &endptr);
    if (endptr == token->start + token->length) {
        atom->type = LD_ATOM_NUMBER;
        atom->value.number = num;
        return LD_OK;
    }
    
    atom->type = LD_ATOM_SYMBOL;
    atom->value.symbol = ld_create_string(token->start, token->length);
    if (!atom->value.symbol)
        return LD_OUT_OF_MEMORY;

    return LD_OK;
}

static enum ld_error_type ld_parse_symbol(struct ld_parser *parser, struct ld_atom *atom, struct ld_token *token) {
    atom->type = LD_ATOM_SYMBOL;
    atom->value.symbol = ld_create_string(token->start, token->length);
    if (!atom->value.symbol)
        return LD_OUT_OF_MEMORY;
    return LD_OK;
}

static enum ld_error_type ld_parse_atom(struct ld_parser *parser, struct ld_atom *atom) {
    enum ld_error_type err = LD_OK;
    struct ld_token token;

    if ((err = ld_next(parser, &token)) != LD_OK)
        return err;
    switch (token.type) {
        case LD_TOKEN_OBJECT:
            return ld_parse_object(parser, atom);
        case LD_TOKEN_ARRAY:
            return ld_parse_array(parser, atom);
        case LD_TOKEN_STRING:
            return ld_parse_string(parser, atom, &token);
        case LD_TOKEN_PRIMITIVE:
            return ld_parse_primitive(parser, atom, &token);
        case LD_TOKEN_SYMBOL:
            return ld_parse_symbol(parser, atom, &token);
        default:
            return LD_UNEXPECTED_TOKEN;
    }
}

void ld_init(struct ld_parser *parser, const char *source, size_t length) {
    parser->source = source;
    parser->length = length;
    parser->position = 0;
};

enum ld_error_type ld_parse(struct ld_parser *parser, struct ld_atom **out, size_t *count) {
    enum ld_error_type err = LD_OK;
    size_t capacity = 8, _count = 0;
    struct ld_atom **atoms = (struct ld_atom**)malloc(capacity * sizeof(struct ld_atom*));
    if (!atoms) {
        err = LD_OUT_OF_MEMORY;
        goto BAIL;
    }

    for (;;) {
        if ((err = skip_whitespace(parser)) != LD_OK)
            goto BAIL;
        if (is_eof(parser))
            break;

        if (_count >= capacity) {
            capacity *= 2;
            struct ld_atom **new_atoms = (struct ld_atom**)realloc(atoms, capacity * sizeof(struct ld_atom*));
            if (!new_atoms) {
                err = LD_OUT_OF_MEMORY;
                goto BAIL;
            }
            atoms = new_atoms;
        }

        struct ld_atom *atom = ld_create_atom();
        if (!atom) {
            err = LD_OUT_OF_MEMORY;
            goto BAIL;
        }

        if ((err = ld_parse_atom(parser, atom)) != LD_OK) {
            ld_free(atom);
            goto BAIL;
        }

        atoms[_count] = atom;
        _count++;
    }

BAIL:
    if (atoms && err != LD_OK) {
        for (size_t i = 0; i < _count; i++)
            ld_free(atoms[i]);
        free(atoms);
    }
    if (out)
        *out = (struct ld_atom*)atoms;
    if (count)
        *count = _count;
    return err;
}

void ld_free(struct ld_atom *atom) {
    if (!atom)
        return;
    switch (atom->type) {
        case LD_ATOM_STRING:
            free(atom->value.string);
            break;
        case LD_ATOM_SYMBOL:
            free(atom->value.symbol);
            break;
        case LD_ATOM_OBJECT:
            if (atom->value.object) {
                for (size_t i = 0; i < atom->value.object->count; i++) {
                    free((void*)atom->value.object->pairs[i].key);
                    ld_free(atom->value.object->pairs[i].value);
                }
                free(atom->value.object->pairs);
                free(atom->value.object);
            }
            break;
        case LD_ATOM_ARRAY:
            if (atom->value.array) {
                for (size_t i = 0; i < atom->value.array->count; i++)
                    ld_free(atom->value.array->items[i]);
                free(atom->value.array->items);
                free(atom->value.array);
            }
            break;
        default:
            break;
    }
    free(atom);
}
#endif // LD_IMPLEMENTATION
