#include "tree_sitter/parser.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

enum TokenType {
    AUTOMATIC_SEMICOLON,
    MULTI_LINE_STRING,
};

typedef struct {
    uint32_t column;
    int32_t lookahead;
    bool emitted;
} Scanner;

void *tree_sitter_onyx_external_scanner_create() {
    Scanner *scanner = calloc(1, sizeof(Scanner));
    return scanner;
}

void tree_sitter_onyx_external_scanner_destroy(void *payload) {
    free(payload);
}

unsigned tree_sitter_onyx_external_scanner_serialize(void *payload, char *buffer) {
    Scanner *scanner = payload;
    buffer[0] = scanner->emitted ? 1 : 0;
    buffer[1] = (char)(scanner->column & 0xff);
    buffer[2] = (char)((scanner->column >> 8) & 0xff);
    buffer[3] = (char)((scanner->column >> 16) & 0xff);
    buffer[4] = (char)((scanner->column >> 24) & 0xff);
    buffer[5] = (char)(scanner->lookahead & 0xff);
    buffer[6] = (char)((scanner->lookahead >> 8) & 0xff);
    buffer[7] = (char)((scanner->lookahead >> 16) & 0xff);
    buffer[8] = (char)((scanner->lookahead >> 24) & 0xff);
    return 9;
}

void tree_sitter_onyx_external_scanner_deserialize(void *payload, const char *buffer, unsigned length) {
    Scanner *scanner = payload;
    if (length < 9) {
        scanner->emitted = false;
        scanner->column = 0;
        scanner->lookahead = 0;
        return;
    }
    scanner->emitted = buffer[0] != 0;
    scanner->column = (uint32_t)(unsigned char)buffer[1]
        | ((uint32_t)(unsigned char)buffer[2] << 8)
        | ((uint32_t)(unsigned char)buffer[3] << 16)
        | ((uint32_t)(unsigned char)buffer[4] << 24);
    scanner->lookahead = (int32_t)(unsigned char)buffer[5]
        | ((int32_t)(unsigned char)buffer[6] << 8)
        | ((int32_t)(unsigned char)buffer[7] << 16)
        | ((int32_t)(unsigned char)buffer[8] << 24);
}

static void skip(TSLexer *lexer) {
    lexer->advance(lexer, true);
}

static bool is_ident_char(int32_t c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static void skip_block_comment(TSLexer *lexer) {
    int depth = 1;
    while (depth > 0 && !lexer->eof(lexer)) {
        if (lexer->lookahead == '/') {
            skip(lexer);
            if (lexer->lookahead == '*') {
                skip(lexer);
                depth++;
            }
        } else if (lexer->lookahead == '*') {
            skip(lexer);
            if (lexer->lookahead == '/') {
                skip(lexer);
                depth--;
            }
        } else {
            skip(lexer);
        }
    }
}

static bool skip_whitespace_and_comments(TSLexer *lexer, bool *saw_newline) {
    for (;;) {
        if (lexer->lookahead == '\n') {
            *saw_newline = true;
            skip(lexer);
            continue;
        }
        if (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\r') {
            skip(lexer);
            continue;
        }
        if (lexer->lookahead == '/') {
            skip(lexer);
            if (lexer->lookahead == '/') {
                while (lexer->lookahead != '\n' && !lexer->eof(lexer)) {
                    skip(lexer);
                }
                continue;
            }
            if (lexer->lookahead == '*') {
                skip(lexer);
                skip_block_comment(lexer);
                continue;
            }
            return true;
        }
        return false;
    }
}

static bool continues_expression(TSLexer *lexer) {
    if (lexer->lookahead == '-') {
        skip(lexer);
        return lexer->lookahead == '>';
    }
    if (lexer->lookahead == '|') {
        skip(lexer);
        return lexer->lookahead == '>';
    }
    if (lexer->lookahead != 'e') return false;

    skip(lexer);
    if (lexer->lookahead != 'l') return false;
    skip(lexer);
    if (lexer->lookahead != 's') return false;
    skip(lexer);
    if (lexer->lookahead != 'e') return false;
    skip(lexer);
    if (lexer->lookahead == 'i') {
        skip(lexer);
        if (lexer->lookahead != 'f') return false;
        skip(lexer);
    }
    return !is_ident_char(lexer->lookahead);
}

static bool scan_automatic_semicolon(Scanner *scanner, TSLexer *lexer) {
    lexer->result_symbol = AUTOMATIC_SEMICOLON;
    lexer->mark_end(lexer);

    bool saw_newline = false;
    bool consumed_slash = skip_whitespace_and_comments(lexer, &saw_newline);

    if (saw_newline && !consumed_slash && continues_expression(lexer)) {
        return false;
    }

    if (saw_newline || lexer->eof(lexer) || (!consumed_slash && lexer->lookahead == '}')) {
        return true;
    }

    return false;
}

static bool scan_multi_line_string(TSLexer *lexer) {
    while (lexer->lookahead == ' ' || lexer->lookahead == '\t' || lexer->lookahead == '\r' || lexer->lookahead == '\n') {
        skip(lexer);
    }

    for (int i = 0; i < 3; i++) {
        if (lexer->lookahead != '"') return false;
        lexer->advance(lexer, false);
    }

    int matched = 0;
    while (!lexer->eof(lexer)) {
        if (lexer->lookahead == '"') {
            matched++;
        } else {
            matched = 0;
        }
        lexer->advance(lexer, false);
        if (matched == 3) {
            lexer->mark_end(lexer);
            lexer->result_symbol = MULTI_LINE_STRING;
            return true;
        }
    }

    return false;
}

bool tree_sitter_onyx_external_scanner_scan(void *payload, TSLexer *lexer, const bool *valid_symbols) {
    Scanner *scanner = payload;

    if (valid_symbols[MULTI_LINE_STRING] && lexer->lookahead == '"') {
        if (scan_multi_line_string(lexer)) return true;
    }

    if (valid_symbols[AUTOMATIC_SEMICOLON]) {
        return scan_automatic_semicolon(scanner, lexer);
    }

    return false;
}
