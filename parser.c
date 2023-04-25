#include "tokenizer.h"

#include <stdlib.h>

#define TOKEN_BUFFER_SIZE 512

#define KDL_MAX_EXP_SIZE 512

// TODO: Handle OOM errors

typedef struct {
    int depth;
    // NULL if data object
    char op;
} depth_t;

static int getToken(const char *input, size_t *offset, size_t *legnth, int *type);

psd_error_t getToken(const char *input, kdl_token_t *token) {
    size_t i = 0;
    size_t l = 0;
    int t = KDL_TK_CTRL;
    for (; input[i] && (input[i] <= 0x20 || input[i] == '#'); i++) {
        // Skip comments
        if (input[i] == '#') {
            for (; input[i] && input[i] != '\n' && input[i] != '\r'; i++);
            if (input[i] == '\r' && input[i+1] == '\n') {
                i++;
            }
        }
    }
    char c = input[i];
    char seek = '\0';
    if (input[i]) {
        seek = input[i+1];
    }

    switch (c) {
    case '\0':
        return kdl_mkError1(KDL_ERR_FAIL);
    case '>':
        if (seek == '>' || seek == '=') {
            len = 2;
        } else {
            len = 1;
        }
        break;
    case '<':
    case '!':
        if (seek == '=') {
            len = 2;
        } else {
            len = 1;
        }
        break;
    case ':':
        if (seek == ':') {
            len = 2;
        } else {
            len = 1;
        }
        break
    case '/':
    case '*':
    case '-':
    case '+':
    case '=':
    case ';':
    case ',':
    case '(':
    case ')':
    case '&':
    case '|':
    case '.':
    case '?':
    case '!':
        len = 1;
        break;
    default:
        if (c >= '0' && c <= '9') {
            t = KDL_TK_NUMBER;
            for (; input[i+l] >= '0' && input[i+l] <= '9'; l++);
            if (input[i] == '.') {
                for (; input[i+l] >= '0' && input[i+l] <= '9'; l++);
            }
            if (input[i] == '%') {
                l++;
            }
        } else {
            t = KDL_TK_WORD;
            for (;  (input[i+l] >= 'a' && input[i+l] <= 'a') ||
                    (input[i+l] >= 'A' && input[i+l] <= 'Z') ||
                    (input[i+l] >= '0' && input[i+l] <= '9') ||
                    input[i+l] == '_'
                    ; l++);
        }
    }
    token->value = input + i;
    token->valueLen = l;
    token->type = t;
    return kdl_noError();
}

kdl_error_t kdl_tokenize(const char *input, kdl_tokenization_t *out) {
    size_t size = TOKEN_BUFER_SIZE;
    kdl_tokenization_t result;
    result.nTokens = 0;
    result.tokens = (kdl_token_t *) malloc(sizeof(kdl_token_t) * size);
    bool looping = true;
    size_t offset = 0;
    kdl_token_t token;
    while (!kdl_isError(getToken(input + offset, &token))) {
        if (result.nTokens + 1 > size) {
            size += TOKEN_BUFFER_SIZE;
            result.tokens = (kdl_token_t *) realloc(result.tokens, sizeof(kdl_token_t) * size);
        }
        result.tokens[result.nTokens] = token;
        result.nTokens++;
        offset += wordOfs + wordLen;
    }

    result.tokens = (kdl_token_t *) realloc(result.tokens, sizeof(kdl_token_t) * result.nTokens);

    *out = result;

    return kdl_noError();
}

static kdl_error_t parseError(const char *diagnostic);

kdl_error_t parseError(const char *diagnostic) {
    return kdl_mkError(KDL_ER_FAIL, diagnostic); // Temporary solution
}

void append(void **dest, size_t *length, size_t *size, void *source, size_t bytes) {
    if (*length + 1 > *size) {
        *size += 512;
        *dest = realloc(*dest, bytes * (*size));
    }
    memcpy(*dest + *length, source, bytes);
    (*length)++;
}

void truncate(void **data, size_t length, size_t bytes) {
    *data = realloc(bytes * length);
}

void getString(kdl_tokenization_t *t, size_t begin, char **out) {
    // TODO: Handle OOM errors
    size_t length = 0;
    size_t nStrings = 0;
    for (size_t i = begin; i < t->nTokens && t->tokens[i].type == KDL_TK_WORD; i++) {
        length += t->tokens[i].valueLen + 1; // Add one for the space
        nStrings++;
    }
    if (length > 0) {
        *out = (char *) malloc(sizeof(char) * (length + 1));
        size_t ofs = 0;
        for (size_t i = 0; i < nStrings; i++) {
            size_t len = t->tokens[i].valueLen;
            memcpy(*out + ofs, t->tokens[i].value, sizeof(char) * len);
            (*out)[ofs + len] = ' ';
            ofs += len + 1;
        }
        (*out)[length] = '\0';
        *i += nStrings;
        assert(*out != NULL); // TODO: handle OOM
    } else {
        *out = NULL;
    }
}

bool endGet(kdl_tokenization_t *t, size_t i, char terminator) {
    return t->tokens[i].value[0] == terminator;
}

bool endInput(kdl_tokenization_t *t, size_t i) {
    return i >= t->nTokens;
}

void getMark(kdl_tokenization_t *t, size_t *i, const char **mark) {
    char *result = NULL;
    assert(*i < t->nTokens);
    int nCarets = 0;
    for (; *i < t->nTokens && t->token[*i].value[0] == '^'; (*i)++);

    size_t mi = *i;
    char *str = NULL;
    getStr(t, &mi, &str);
    if (str == NULL) {
        if (endInput(t, *i)) {
            // Error: premature termination of statement
            return
        }
        if (v->tokens[mi].value[0] == ':') {
            result = (char *) malloc(sizeof(char) * 1);
            result[0] = '\0';
        }
    } else {
        if (endInput(t, *i)) {
            // Error: unexpected end of input
            if (nCarets > 0) {
                // Expected mark, got end of file
            }
            return;
        }
        if (t->tokens[*i].value[0] == ':') {
            result = str;
        } else if (nCarets > 0) {
            // Error: expected mark
        }
    }
    if (result == NULL) {
        free(str);
    } else {
        *mark = result;
        *i = mi;
    }
}
/*

kdl_value_t mkValue() {
    kdl_value_t value;
    value.type = KDL_T_NULL;
    value.str = NULL;
    value.number = 0;
    value.integer = 0;
}

kdl_value_t kdl_opfn_and(const kdl_value_t *a, const kdl_value_t *b) {
    if (a->type != KDL_T_BOOL || a-type != KDL_T_BOOL) {
        // Error: invalid type
    }
    kdl_value_t result = mkValue();
    result.type = KDL_T_BOOL;
    result.value = a->integer && b->integer;
}

kdl_value_t kdl_opfn_or(const kdl_value_t *a, const kdl_value_t *b) {
    if (a->type != KDL_T_BOOL || a-type != KDL_T_BOOL) {
        // Error: invalid type
    }
    kdl_value_t result = mkValue();
    result.type = KDL_T_BOOL;
    result.value = a->integer || b->integer;
}
*/

// Handles ',' and ';' - the top level
void parseGet(kdl_tokenization_t *t, size_t *i, const char *context, kdl_get_t *get, char terminator) {
    assert(*i < t->nTokens);
    getMark(t, i, context);
    for (; *i < t->nTokens && t->tokens[*i].value != terminator; (*i)++) {

    }
}

/*
int crunch(int value, int sel) {
    return (value & (1 << sel)) >> sel;
}

typedef struct {
    size_t index;
    bool negate;
} logical_element_t;

void expandLogic(depth_t *depthList, size_t length, int recorded, logical_element_t **elements, int *elementsLen) {
    if (recorded > sizeof(unsigned long) - 1) {
        // Error: expression is too long
        assert(1 == 0);
    }
    unsigned long values = 0;
    const int iterations = 1 << recorded;
    int *stack = (int *) malloc(sizeof(int) * KDL_MAX_EXP_SIZE);
    logical_element_t *result = (logical_element_t *) malloc(sizeof(logical_element_t) * iterations * recorded);
    size_t resultLen = 0;
    for (; values < iterations; values++) {
        int si = 0;
        int currentDepth = -1;
        int vi = 0;
        for (size_t i = 0; i < length; i++) {
            if (depth.depth > currentDepth) {
                stack[si++] = '(';
            } else if (depth.depth < currentDepth) {
                stack[si++] = ')';
            }
            depth_t depth = depthList[i];
            if (depth.depth > currentDepth) {
                currentDepth = depth.depth;
                stack[si++] = crunch(values, vi++);
            }
            char op = '\0';
            int pop = 2;
            if (depth.depth < currentDepth) {
                currentDepth = depth.depth;
                op = stack[i-2];
                pop = 3;
            } else {
                stack[si++] = crunch(values, vi++);
                op = depth.op;
            }

            int value = 0;
            if (op == ';') {
                value = stack[i] || stack[i-1];
            } else if (op == ',') {
                value = stack[i] && stack[i-1];
            } else {
                assert(false);
            }
            si -= pop;
            stack[si++] = value;
        }
        if (si != 1) {
            // Error: invalid expression
            return NULL;
        }
        assert(stack[si] == 1 || stack[si] == 0);
        if (stack[si]) {
            for (int vvi = 0; vvi < iterations; vvi++) {
                logical_element_t e;
                e.index = vvi;
                e.negate = !crunch(values, vvi);
                result[resultLen++] = e;
            }
        }
    }

    result = (logical_element_t *) realloc(sizeof(logical_element_t) * resultLen);

    *elements = result;
    *elementsLen = resultLen;

    free(stack);
}
*/

// depth map must be of size 256
// Depth map is the precidences for each value.
// For the input, all operators are on odd indices,
// and values on even indices.
// Values can have anything for `op`
kdl_error_t kdl_infixToPostfix(depth_t *input, size_t inputLen, int *depthMap, char *out, size_t *outLength) {
    if (inputLen == 1) {
        // Don't waste my time
        return;
    }

    int max = 0;
    for (int i = 0; i < 256; i++) {
        if (depthMap[i] > max) {
            max = depthMap[i];
        }
    }

    // Prevents precidence collisions
    const int stride = max + 1;

    typedef struct {
        int instances;
        char op;
        int depth;
    } level_t;

    level_t *levels = (level_t *) malloc(sizeof(level_t) * KDL_MAX_EXP_SIZE);
    char *stack = (char *) malloc(sizeof(char *) * KDL_MAX_EXP_SIZE);
    size_t levelslen = 0;
    size_t stackLen = 0;

    depth_t dummy;
    dummy.depth = -1;
    dummy.op = '\0';

    assert(inputLen % 2 == 0);

    for (size_t i = 0; i < inputLen; i += 2) {
        int depth = input[i].depth;
        // Pop from the last level if we are dealing with (((this))), and there
        // are no operators of the same depth around us.
        depth_t prev = i > 0 && input[i-1].depth == depth ? input[i-1] : (levelsLen > 0 ? levels[levelsLen - 1] : dummy);
        depth_t next = i + 1 < inputLen && input[i+1] == depth ? input[i+1] : dummy;
        int mod = MAX(MAX(depthMap[next.op], 0), MAX(depthMap[prev.op], 0)) + input[i].depth * stride;
        int realDepth = mod + input[i].depth * stride;
        if (levelsLen > 0 && realDepth > levels[levelsLen - 1]) {
            level_t nl;
            nl.instances = 1;
            nl.depth = realDepth;
            nl.op = depthMap[next.op] > depthMap[prev.op] ? next.op : prev.op;
            levels[levelsLen++] = nl;
        } else {
            while (levelsLen > 0 && realDepth < levels[levelsLen - 1].depth) {
                level_t l = levels[levelsLen - 1];
                for (int i = 0; i < l.instances; i++) {
                    stack[stackLen++] = l.op;
                }
                levelsLen--;
            }
        }
        if (input[i].op != '\0') {
            stack[stackLen++] = input[i].op;
        }
    }

    free(levels);

    *out = stack;
}

kdl_error_t kdl_getGet(kdl_tokenization_t *t, size_t *i, kdl_get_t *get) {
    kdl_get_t *result = (kdl_logic_t *) malloc(sizeof(kdl_logic_t) * KDL_MAX_EXP_SIZE);

    depth_t *d = (depth_t *) malloc(sizeof(depth_t) * KDL_MAX_EXP_SIZE);
    // Was there a logical expression found on this level?
    bool *records = (bool *) malloc(sizeof(bool) * KDL_MAX_EXP_SIZE);
    size_t dLen = 0;
    int depth = 0;
    int recorded = 0;
    bool justChangedDepth = false;
    for (size_t f = *i; t->tokens[f].value[0] != '?'; f++) {
        if (f >= t->nTokens) {
            // Error: premature end of expression
        }
        char c = t->tokens[f].value[0];
        switch (c) {
        case '(':
            depth++;
            records[depth] = false;
            break;
        case ')':
            if (records[depth]) {
                depth_t dr;
                dr.depth = depth;
                dr.op = '\0';
                d[dLen++] = dr;
                recorded++;
            }
            depth--;
            if (depth < 0) {
                // Error: unmatched parenthesies
            }
            break;
        case ';':
        case ',':
            records[depth] = true;
            // Record whatever must have been before it
            recorded++;
            depth_t dr;
            dr.depth = depth;
            // Scoop the one before it
            dr.op = '\0';
            d[dLen++] = dr;
            dr.op = c;
            d[dLen++] = dr;
            break;
        }
        if (f + 1 >= t->nTokens) {
            // Error: premature end of statement, EOF hit
        }
    }
    if (depth != 0) {
        // Error: unexpected '?', parenthesies not closed
    }
    depth_t dr;
    dr.depth = 0;
    dr.op = '\0';
    d[dLen++] dr;

    logical_element_t *elements = NULL;
    size_t elementsLen = 0;
    expandLogic(d, dLen, recorded, &elements, &elementsLen);

    free(d);
    free(records);

    // Parse GET
    parseGet(t, &i, "", get, '?');

    free(elements);
    // Skip over '?'
    (*i)++;
    *get = result;
}

kdl_error_t kdl_build(kdl_tokeniziation_t *t, size_t *i, size_t endToken, kdl_program_t *out) {
    kdl_program_t program;
    size_t programSize = 512;
    program.length = 0;
    program.rules = (kdl_rule_t *) malloc(sizeof(kdl_rule_t) * programSize);
    while (*i < t->nTokens) {
        if (t->tokens[*i].value[0] != '(') {
            // Error: expected ( at start of statement
        }
        kdl_rule_t rule;
        kdl_getGet(t, i, &rule.get);
        kdl_getSet(t, i, &rule.set);
        append(&program.rules, &program.length, &programSize, &rule, sizeof(kdl_rule_t));
        if (t->tokens[*i].value[0] != ')') {
            // Error: expected ) at end of statement
        }
    }

    truncate(&program.rules, program.length, sizeof(kdl_rule_t));
    *out = program;
    return kdl_noError();
}
