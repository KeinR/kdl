#include "parser.h"

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <ctype.h>
#include <assert.h>

#define MAX_STRING_LEN 1028

#define TOKEN_BUFFER_SIZE 512
#define KDL_MAX_EXP_SIZE 512
#define MAX_CONTEXT_DEPTH 512
#define CONTEXT_BUFFER_SIZE MAX_STRING_LEN
#define MAX_COMPUTE_PARAMS 512
#define MAX_EXECUTE_PARAMS 512
#define PROGRAM_BUFFER_SIZE 512

#define MAX_PREC 20

#define ERROR_START kdl_error_t _err = noError();
#define ERROR_IS goto noerror; error:
#define ERROR_NONE noerror:
#define ERROR_END end: return _err;

#define NO_ERROR goto end;
#define ERROR(a,b,c) _err = mkError(a,b,(c).value,(c).valueLen,true); goto error;
#define NTERROR(a,b,c) _err = mkError(a,b,c,0,false); goto error;

#define TEST(exp) if (isError(_err = (exp))) goto error;

// TODO: Handle OOM errors
// TODO: Use isspace instead of checking for whitespace ourselves

typedef struct {
    // The precidence change inflicted on child elements.
    // If this is anything but zero, this element is excluded from
    // the output, and `prec`'s side effects are nullified.
    // Commonly, '(' would have this be 1, and ')' as -1
    int precChange;
    // The precidence. Higher means higher precidence. If this is <= 0, this is data.
    // If this is greater than zero, it is an operator. That is to say, this is OPERATOR precidence.
    int prec;
    // User data associated with this element
    void *data;
} element_t;

typedef struct {
    char *context;
    size_t contextLen;
    size_t *indices;
    size_t indicesLen;
    int depth;
} contextTracker_t;

static kdl_error_t getRawValue(kdl_state_t s, kdl_token_t token, void **outValue, int *outOp, bool *outGlobal);
static kdl_error_t getMark(kdl_state_t s, contextTracker_t context, int depth, kdl_tokenization_t *t, size_t *i, contextTracker_t *out, bool *got);
static kdl_error_t mkError(int code, const char *message, const char *pointer, size_t length, bool hasLength);
static kdl_error_t noError();
static bool isError(kdl_error_t error);
static void getContextString(kdl_state_t s, contextTracker_t tracker, char **out);
static bool tokenEqChar22(kdl_token_t t, char a, char b, int type);
static bool tokenEqCharNT(kdl_token_t t, char c);
static bool tokenEqChar(kdl_token_t t, char c, int type);
static void freeTokenization(kdl_state_t s, kdl_tokenization_t *t);
static void freeOp(kdl_state_t s, kdl_op_t *o);
static void freeCompute(kdl_state_t s, kdl_compute_t *c);
static void freeAction(kdl_state_t s, kdl_action_t *a);
static void freeExecute(kdl_state_t s, kdl_execute_t *e);
static void freeRule(kdl_state_t s, kdl_rule_t *e);
static kdl_error_t addToContext(kdl_token_t currentToken, contextTracker_t *tracker, const char *word, size_t wordLen);
static void mkContext(kdl_state_t s, int depth, contextTracker_t *out);
static void freeContext(kdl_state_t s, contextTracker_t *tracker);
static kdl_error_t getContext(kdl_state_t s, kdl_token_t currentToken, int depth, contextTracker_t base, size_t levels, contextTracker_t *out);
static bool isNumber(char c);
static bool isCharOrUns(char c);
static kdl_error_t getToken(const char *input, kdl_token_t *token, size_t *skip, bool *eof);
static void createStringCopyNoWhitespace(kdl_state_t s, const char *input, size_t length, char **out);
static void infixToPostfix(kdl_state_t s, element_t *input, size_t inputLen, int maxPrec, void ***out, size_t *outLength);
static kdl_error_t tokenize(kdl_state_t s, const char *input, kdl_tokenization_t *out);
static kdl_error_t getCompute(kdl_state_t s, contextTracker_t parent, kdl_tokenization_t *t, size_t *i, kdl_compute_t *out, char terminate);
static kdl_error_t getValue(kdl_state_t s, contextTracker_t parentContext, kdl_tokenization_t *t, size_t *i, kdl_compute_t *out);
static kdl_error_t getExecute(kdl_state_t s, contextTracker_t parentContext, kdl_tokenization_t *t, size_t *i, kdl_execute_t *out);
static kdl_error_t getRule(kdl_state_t s, contextTracker_t context, kdl_tokenization_t *t, size_t *i, kdl_rule_t *rule);
static kdl_error_t getProgram(kdl_state_t s, contextTracker_t context, kdl_tokenization_t *t, size_t *i, kdl_program_t *out, char terminate);

// Parse value literal (consumes single token)
kdl_error_t getRawValue(kdl_state_t s, kdl_token_t token, void **outValue, int *outOp, bool *outGlobal) {
    ERROR_START

    assert(token.type != KDL_TK_CTRL);

    int op = KDL_OP_NOOP;
    void *value = NULL;
    char *string = NULL;
    bool global = false;
    createStringCopyNoWhitespace(s, token.value, token.valueLen, (char **) &string);
    switch(token.type) {
    case KDL_TK_WORD:
        // Variable, by process of elimination.
        // locally scoped.
        op = KDL_OP_PVAR;
        break;
    case KDL_TK_INT:
        value = s.malloc(sizeof(kdl_int_t));
        *((kdl_int_t *)value) = (kdl_int_t) strtoll(string, NULL, 10);
        if (errno == ERANGE) {
            ERROR(KDL_ERR_VAL, "Integer too large", token)
        }
        op = KDL_OP_PINT;
        break;
    case KDL_TK_FLOAT:
        value = s.malloc(sizeof(kdl_float_t));
        *((kdl_float_t *) value) = (kdl_float_t) strtold(string, NULL);
        if (errno == ERANGE) {
            ERROR(KDL_ERR_VAL, "Floating point number too large", token)
        }
        op = KDL_OP_PFLOAT;
        break;
    case KDL_TK_PERC:
        value = s.malloc(sizeof(kdl_float_t));
        // Docs say that strtold() doesn't care if there's junk at the end
        // of the input.
        // And I suppose that goes for our trailing %
        // And of course, we know that the tokenizer has validated the input
        *((kdl_float_t *) value) = (kdl_float_t) strtold(string, NULL);
        if (errno == ERANGE) {
            ERROR(KDL_ERR_VAL, "Percentage value too large", token)
        }
        op = KDL_OP_PPERC;
        break;
    case KDL_TK_VAR: {
        // The braces make it implicity global
        global = true;
        size_t offset = 0;
        size_t doOffset = 0;
        for (; isspace(token.value[offset]) && offset < token.valueLen; offset++);
        if (token.value[offset] == '>') {
            global = false;
            // Skip the >, we already have the information we need
            doOffset = offset + 1;
        }
        createStringCopyNoWhitespace(s, token.value + doOffset, token.valueLen - doOffset, (char **) &value);
        op = KDL_OP_PVAR;
        break;
    }
    case KDL_TK_STR:
        op = KDL_OP_PSTR;
        break;
    default:
        // This is the full extent of our tokenizer's tokens
        assert(false);
    }

    if (value == NULL) {
        value = string;
    } else {
        s.free(string);
    }

    *outValue = value;
    *outOp = op;
    *outGlobal = global;

    ERROR_IS

    s.free(value);
    s.free(string);

    ERROR_NONE

    ERROR_END
}

// Get context marker
// `got`: if the get was successful, and `i` was incremented
// Format:
// ^* <WORDS> :
kdl_error_t getMark(kdl_state_t s, contextTracker_t context, int depth, kdl_tokenization_t *t, size_t *i, contextTracker_t *out, bool *got) {
    ERROR_START

    *got = false;

    size_t f = *i;
    contextTracker_t nc;
    memset(&nc, 0, sizeof(contextTracker_t));

    size_t jumpers = 0;
    while (tokenEqChar(t->tokens[f], '^', KDL_TK_CTRL)) {
        jumpers++;
        f++;
        if (f >= t->nTokens) {
            ERROR(KDL_ERR_EOF, "Reached EOF while reading mark jumpers", t->tokens[f-1])
        }
    }


    if (jumpers == 0) {
        mkContext(s, depth, &nc);
    } else {
        TEST(getContext(s, t->tokens[f], depth, context, jumpers - 1, &nc))
    }

    while (t->tokens[f].type == KDL_TK_WORD) {
        kdl_token_t token = t->tokens[f];
        TEST(addToContext(token, &nc, token.value, token.valueLen))
        f++;
        if (f >= t->nTokens) {
            ERROR(KDL_ERR_EOF, "Reached EOF while reading mark", t->tokens[f-1])
        }
    }

    if (tokenEqChar(t->tokens[f], ':', KDL_TK_CTRL)) {
        f++;
        *i = f;
        *out = nc;
        *got = true;
    } else if (jumpers > 0) {
        ERROR(KDL_ERR_EXP, "Expected ':' at end of mark", t->tokens[f])
    } else {
        freeContext(s, &nc);
    }


    ERROR_IS

    freeContext(s, &nc);

    ERROR_NONE

    ERROR_END
}

kdl_error_t mkError(int code, const char *message, const char *pointer, size_t length, bool hasLength) {
    kdl_error_t e;
    e.code = code;
    e.message = message;
    e.data = pointer;
    e.dataLen = length;
    e.hasDataLen = hasLength;
    return e;
}

kdl_error_t noError() {
    return mkError(KDL_ERR_OK, "No error", NULL, 0, true);
}

bool isError(kdl_error_t error) {
    return error.code != KDL_ERR_OK;
}

// Get the null-terminated string representation of a context.
void getContextString(kdl_state_t s, contextTracker_t tracker, char **out) {
    *out = (char *) s.malloc(sizeof(char) * (tracker.contextLen + 1));
    memcpy(*out, tracker.context, sizeof(char) * tracker.contextLen);
    (*out)[tracker.contextLen] = '\0';
}

bool tokenEqChar22(kdl_token_t t, char a, char b, int type) {
    return t.valueLen == 2 && t.value[0] == a && t.value[1] == b && t.type == type;
}

bool tokenEqCharNT(kdl_token_t t, char c) {
    return t.valueLen == 1 && t.value[0] == c;
}

bool tokenEqChar(kdl_token_t t, char c, int type) {
    return t.type == type && t.valueLen == 1 && t.value[0] == c;
}

void freeTokenization(kdl_state_t s, kdl_tokenization_t *t) {
    // Tokens do not hold onto memory, just pointer offsets in the input string
    s.free(t->tokens);
    memset(t, 0, sizeof(kdl_tokenization_t));
}

void freeOp(kdl_state_t s, kdl_op_t *o) {
    s.free(o->context);
    s.free(o->value);
    memset(o, 0, sizeof(kdl_op_t));
}

void freeCompute(kdl_state_t s, kdl_compute_t *c) {
    for (size_t i = 0; i < c->length; i++) {
        freeOp(s, &c->opers[i]);
    }
    memset(c, 0, sizeof(kdl_compute_t));
}

void freeProgram(kdl_state_t s, kdl_program_t *p) {
    for (size_t i = 0; i < p->length; i++) {
        freeRule(s, &p->rules[i]);
    }
    s.free(p->rules);
    memset(p, 0, sizeof(kdl_program_t));
}

void freeAction(kdl_state_t s, kdl_action_t *a) {
    s.free(a->context);
    s.free(a->verb);
    for (size_t i = 0; i < a->nParams; i++) {
        freeCompute(s, &a->params[i]);
    }
    s.free(a->params);
    memset(a, 0, sizeof(kdl_action_t));
}

void freeExecute(kdl_state_t s, kdl_execute_t *e) {
    freeProgram(s, &e->child);
    freeAction(s, &e->order);
    memset(e, 0, sizeof(kdl_execute_t));
}

void freeRule(kdl_state_t s, kdl_rule_t *e) {
    freeExecute(s, &e->execute);
    freeCompute(s, &e->compute);
    memset(e, 0, sizeof(kdl_rule_t));
}

// Add a single word to the context of a given length.
// Current token is given for error handling only.
kdl_error_t addToContext(kdl_token_t currentToken, contextTracker_t *tracker, const char *word, size_t wordLen) {
    ERROR_START

    if (tracker->indicesLen + 1 > CONTEXT_BUFFER_SIZE) {
        ERROR(KDL_ERR_BUF, "Context depth buffer size exceeded", currentToken)
    }
    if (tracker->contextLen + wordLen + 1 > CONTEXT_BUFFER_SIZE) {
        ERROR(KDL_ERR_BUF, "Context string buffer size exceeded", currentToken)
    }
    tracker->context[tracker->contextLen++] = ' ';
    memcpy(tracker->context + tracker->contextLen, word, wordLen);
    tracker->indices[tracker->indicesLen++] = tracker->contextLen;
    tracker->contextLen += wordLen;

    ERROR_IS

    ERROR_NONE

    ERROR_END
}

// Make a fresh context
void mkContext(kdl_state_t s, int depth, contextTracker_t *out) {
    out->context = (char *) s.malloc(sizeof(char) * CONTEXT_BUFFER_SIZE);
    out->indices = (size_t *) s.malloc(sizeof(size_t) * CONTEXT_BUFFER_SIZE);
    out->contextLen = 0;
    out->indicesLen = 0;
    out->depth = depth;
}

void freeContext(kdl_state_t s, contextTracker_t *out) {
    s.free(out->context);
    s.free(out->indices);
    memset(out, 0, sizeof(contextTracker_t));
}

// Initialize context based off of a given context.
// The OUT parameter must be blank (we allocate fresh memory here)
// Levels is the number of words (scopes, whatever) that should be discarded.
// Note that surpassing the number of words with `levels` will result in a
// fresh, blank context being returned
kdl_error_t getContext(kdl_state_t s, kdl_token_t currentToken, int depth, contextTracker_t base, size_t levels, contextTracker_t *out) {
    ERROR_START

    contextTracker_t result;
    mkContext(s, depth, &result);
    if (levels < base.indicesLen) {
        TEST(addToContext(currentToken, &result, base.context, levels > 0 ? base.indices[base.indicesLen - levels] : base.contextLen))
    }
    *out = result;

    ERROR_IS

    freeContext(s, &result);

    ERROR_NONE

    ERROR_END
}

bool isNumber(char c) {
    return c >= '0' && c <= '9';
}

bool isCharOrUns(char c) {
    return (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z') ||
           c == '_';
}

// `eof` is set to true if we reached EOF.
// `input` must be null-terminated.
kdl_error_t getToken(const char *input, kdl_token_t *out, size_t *skip, bool *eof) {
    ERROR_START

    size_t i = 0;
    size_t l = 0;
    size_t s = 0;
    int t = KDL_TK_CTRL;
    for (; input[i] && (isspace(input[i]) || input[i] == '#'); i++) {
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
    if (c) {
        seek = input[i+1];
    } else {
        *eof = true;
        NO_ERROR
    }

    assert(c != ' ');

    switch (c) {
    case '>':
    case '<':
    case '!':
        if (seek == '=') {
            l = 2;
        } else {
            l = 1;
        }
        break;
    case ':':
        if (seek == ':') {
            l = 2;
        } else {
            l = 1;
        }
        break;
    case '/':
    case '*':
    case '-':
    case '+':
    case '=':
    case ';':
    case ',':
    case '(':
    case ')':
    case '?':
        l = 1;
        break;
    case '[':
        t = KDL_TK_STR;
        i++;
        // TODO: Die on invalid characters
        for (; input[i+l] != ']'; l++) {
            if (!input[i+l]) {
                NTERROR(KDL_ERR_UNX, "Expected something other than EOF while reading string literal", input)
            }
        }
        s = 1;
        break;
    case '{':
        t = KDL_TK_VAR;
        i++;
        // TODO: Die on invalid characters
        for (; input[i+l] != '}'; l++) {
            if (!input[i+l]) {
                NTERROR(KDL_ERR_UNX, "Expected something other than EOF while reading variable", input)
            }
        }
        s = 1;
        break;
    default:
        // Does not support eg. '.4235'
        // And it shouldn't!
        if (isNumber(c)) {
            t = KDL_TK_INT;
            for (; isNumber(input[i+l]); l++);
            if (input[i] == '.') {
                t = KDL_TK_FLOAT;
                for (; isNumber(input[i+l]); l++);
            }
            if (input[i] == '%') {
                t = KDL_TK_PERC;
                l++;
            }
        } else if (isCharOrUns(input[i+l])) {
            t = KDL_TK_WORD;
            for (; isCharOrUns(input[i+l]); l++);
        } else {
            printf("\n%i\n", (int) c);
            NTERROR(KDL_ERR_UNX, "Unrecognized character", input)
        }
    }
    // LOOK AT THIS
    // NO ALLOCATIONS
    out->value = input + i;
    out->valueLen = l;
    out->type = t;
    *skip = s;

    ERROR_IS

    ERROR_NONE

    ERROR_END
}

// Create a null-terminated string from a non-null terminated string and length
void createStringCopyNoWhitespace(kdl_state_t s, const char *input, size_t length, char **out) {
    char *result = (char *) s.malloc(sizeof(char) * (length + 1));
    size_t resultLen = 0;
    for (size_t i = 0; i < length; i++) {
        if (!isspace(input[i])) {
            result[resultLen++] = input[i];
        } else if (resultLen > 0 && !isspace(result[resultLen - 1])) {
            // Admitidly this also means we admit funny characters.
            // This also means that we admit funny characters.
            result[resultLen++] = ' ';
        }
    }
    result[resultLen++] = '\0';
    result = (char *) s.realloc(result, sizeof(char) * resultLen);

    *out = result;
}


// Convert an expression composed of element_t's of length inputLen with
// a cumulitive max precidence of maxPrec (MUST be correct!!!) from infix
// notation to postifx notation.
// !! Zero length input OK!!
void infixToPostfix(kdl_state_t s, element_t *input, size_t inputLen, int maxPrec, void ***out, size_t *outLength) {
    if (inputLen == 0) {
        *out = NULL;
        *outLength = 0;
        return;
    }

    typedef struct {
        int realPrec;
        void *data;
    } level_t;

    // Prevents precidence collisions
    const int precStride = maxPrec + 1;

    level_t *levels = (level_t *) s.malloc(sizeof(element_t) * inputLen);
    void  **stack = (void **) s.malloc(sizeof(void *) * inputLen);
    size_t levelsLen = 0;
    size_t stackLen = 0;

    int currentPrec = 0;
    for (size_t i = 0; i < inputLen; i++) {
        element_t e = input[i];
        assert(e.prec <= maxPrec);
        if (e.precChange != 0) {
            // Parenthesies
            currentPrec += e.precChange * precStride;
        } else if (e.prec > 0) {
            // Operator
            int realPrec = e.prec + currentPrec;

            // While we are going up the precidence tree, leaving depth
            while (levelsLen > 0 && realPrec < levels[levelsLen - 1].realPrec) {
                stack[stackLen++] = levels[levelsLen - 1].data;
                levelsLen--;

                assert(stackLen <= inputLen);
            }

            level_t l;
            l.realPrec = realPrec;
            l.data = e.data;
            levels[levelsLen++] = l;

            assert(levelsLen <= (inputLen - 1) / 2);
        } else {
            // Value
            stack[stackLen++] = input[i].data;

            assert(stackLen <= inputLen);
        }
    }

    while (levelsLen > 0) {
        stack[stackLen++] = levels[--levelsLen].data;
    }

    s.free(levels);

    stack = s.realloc(stack, sizeof(void *) * stackLen);

    *out = stack;
    *outLength = stackLen;
}

// Perform the tokenization of the string
kdl_error_t tokenize(kdl_state_t s, const char *input, kdl_tokenization_t *out) {
    ERROR_START

    size_t size = TOKEN_BUFFER_SIZE;
    kdl_tokenization_t result;
    result.nTokens = 0;
    result.tokens = (kdl_token_t *) s.malloc(sizeof(kdl_token_t) * size);
    kdl_token_t token;
    const char *offset = input;
    bool eof = false;
    while (true) {
        size_t skip = 0;
        TEST(getToken(offset, &token, &skip, &eof))
        if (eof) {
            break;
        }
        if (result.nTokens + 1 > size) {
            size += TOKEN_BUFFER_SIZE;
            result.tokens = (kdl_token_t *) s.realloc(result.tokens, sizeof(kdl_token_t) * size);
        }
        result.tokens[result.nTokens++] = token;
        offset = token.value + token.valueLen + skip;
    }

    result.tokens = (kdl_token_t *) s.realloc(result.tokens, sizeof(kdl_token_t) * result.nTokens);

    *out = result;

    ERROR_IS

    s.free(result.tokens);

    ERROR_NONE

    ERROR_END
}

kdl_error_t getCompute(kdl_state_t s, contextTracker_t parent, kdl_tokenization_t *t, size_t *i, kdl_compute_t *out, char terminate) {
    ERROR_START

    contextTracker_t *contexts = (contextTracker_t *) s.malloc(sizeof(contextTracker_t) * MAX_CONTEXT_DEPTH);
    element_t *elements = (element_t *) s.malloc(sizeof(element_t) * KDL_MAX_EXP_SIZE);
    kdl_op_t **stack = NULL;
    size_t contextsLen = 0;
    size_t elementsLen = 0;
    size_t stackLen = 0;

    // `parent` is never free'd, it is only here to provide for lookbehinds;
    // We are not allowed to free `parent`
    contexts[contextsLen++] = parent;

    // Loop variables
    void *value = NULL;
    int depth = parent.depth;
    bool loop = true;

    for (; loop && *i < t->nTokens; (*i)++) {
        kdl_token_t token = t->tokens[*i];
        if (elementsLen >= KDL_MAX_EXP_SIZE) {
            ERROR(KDL_ERR_BUF, "Expression buffer size exceeded (elements/operations)", token)
        }
        if (tokenEqChar(token, terminate, KDL_TK_CTRL) && terminate != ')') {
            (*i)++;
            break;
        }

        value = NULL;
        bool global = false;

        // TODO: take variables, create object in `if (loop) { ... }`
        element_t e;
        e.prec = 0;
        e.precChange = 0;
        e.data = NULL;
        // -- end TODO
        int op = KDL_OP_NOOP;
        // Switch with two options
        // A literal switch lol
        switch(token.type) {
        case KDL_TK_CTRL:
            if (token.valueLen == 1) {
                switch(token.value[0]) {
                case '+':
                    op = KDL_OP_ADD;
                    e.prec = 4;
                    break;
                case '-':
                    op = KDL_OP_SUB;
                    e.prec = 5;
                    break;
                case '/':
                    op = KDL_OP_DIV;
                    e.prec = 7;
                    break;
                case '*':
                    op = KDL_OP_MUL;
                    e.prec = 6;
                    break;
                case '=':
                    op = KDL_OP_EQU;
                    e.prec = 3;
                    break;
                case '<':
                    op = KDL_OP_LTH;
                    e.prec = 3;
                    break;
                case '>':
                    op = KDL_OP_GTH;
                    e.prec = 3;
                    break;
                case ',':
                    op = KDL_OP_AND;
                    e.prec = 2;
                    break;
                case ';':
                    op = KDL_OP_OR;
                    e.prec = 1;
                    break;
                case '!':
                    op = KDL_OP_NOT;
                    e.prec = 8;
                    break;
                case '(': {
                    depth++;
                    e.precChange = 1;
                    bool got = false;
                    contextTracker_t nc;
                    size_t ti = *i + 1;
                    TEST(getMark(s, contexts[contextsLen - 1], depth, t, &ti, &nc, &got))
                    if (got) {
                        // getMark, like everyone else, puts us just after its
                        // content. The loop increments i by one every time, so
                        // to get the next token we must decrement by one.
                        *i = ti - 1;
                        contexts[contextsLen++] = nc;
                    }
                    break;
                }
                case ')':
                    depth--;
                    e.precChange = -1;
                    assert(contextsLen > 0);
                    if (depth <= contexts[contextsLen - 1].depth && contextsLen == 1) {
                        assert(depth <= parent.depth); // Same thing
                        if (terminate == ')') {
                            loop = false;
                        } else {
                            ERROR(KDL_ERR_UNX, "Unmatched ')'", t->tokens[*i])
                        }
                    } else {
                        assert(contextsLen != 1);
                        freeContext(s, &contexts[--contextsLen]);
                    }
                    break;
                default:
                    assert(false);
                }
            } else if (token.valueLen == 2) {
                if (token.value[0] == '<' && token.value[1] == '=') {
                    op = KDL_OP_LEQ;
                    e.prec = 3;
                } else if (token.value[0] == '>' && token.value[1] == '=') {
                    op = KDL_OP_GEQ;
                    e.prec = 3;
                } else {
                    assert(false);
                }
            } else {
                assert(false);
            }
            break;
        default:
            TEST(getRawValue(s, token, &value, &op, &global))
        }
        if (loop && op != KDL_OP_NOOP) {
            if (value == NULL) {
                createStringCopyNoWhitespace(s, token.value, token.valueLen, (char **) &value);
            }

            kdl_op_t *opv = NULL;
            opv = (kdl_op_t *) malloc(sizeof(kdl_op_t));
            opv->op = op;
            opv->value = value;
            if (global) {
                opv->context = NULL;
            } else {
                getContextString(s, contexts[contextsLen - 1], &opv->context);
            }
            e.data = opv;
            elements[elementsLen++] = e;
        } else {
            s.free(value);
            value = NULL;
        }
        assert(e.prec <= MAX_PREC);
    }



    infixToPostfix(s, elements, elementsLen, MAX_PREC, (void ***) &stack, &stackLen);

    // Flatten the stack.
    // Supposidly this improves CPU cache or smthn idk.
    // Well I like flat arrays so...!!!!
    kdl_compute_t compute;
    compute.opers = (kdl_op_t *) s.malloc(sizeof(kdl_op_t) * stackLen);
    compute.length = stackLen;

    for (size_t f = 0; f < stackLen; f++) {
        compute.opers[f] = *(stack[f]);
    }

    *out = compute;

    ERROR_IS

    for (size_t f = 0; f < stackLen; f++) {
        s.free(stack[f]->context);
        s.free(stack[f]->value);
    }

    ERROR_NONE

    for (size_t f = 1; f < contextsLen; f++) {
        freeContext(s, &contexts[f]);
    }

    for (size_t f = 0; f < stackLen; f++) {
        s.free(stack[f]);
    }

    s.free(contexts);
    s.free(elements);
    s.free(stack);

    ERROR_END
}

// For the parameters to the verb
kdl_error_t getValue(kdl_state_t s, contextTracker_t parentContext, kdl_tokenization_t *t, size_t *i, kdl_compute_t *out) {
    ERROR_START

    kdl_token_t token = t->tokens[*i];
    kdl_compute_t result;
    memset(&result, 0, sizeof(kdl_compute_t));
    if (token.type == KDL_TK_CTRL && !tokenEqChar(token, '(', KDL_TK_CTRL)) {
        ERROR(KDL_ERR_UNX, "Unexpected control character at beginning of supposed value expression", token)
    }
    if (token.type == KDL_TK_CTRL) {
        // Thus, must be (, and so we can forward it...
        // (*i)++; getCompute needs the parenthesies
        TEST(getCompute(s, parentContext, t, i, &result, ')'))
        // (*i)++; getCompute consumes everything
    } else {
        // Must be single-token literal, then
        kdl_op_t op;
        bool global = false;
        TEST(getRawValue(s, token, &op.value, &op.op, &global))
        if (global) {
            op.context = NULL;
        } else {
            getContextString(s, parentContext, &op.context);
        }
        (*i)++;
        result.length = 1;
        result.opers = (kdl_op_t *) s.malloc(sizeof(kdl_op_t) * result.length);
        result.opers[0] = op;
    }

    *out = result;

    ERROR_IS

    ERROR_NONE

    ERROR_END
}

// The verb et al.
kdl_error_t getExecute(kdl_state_t s, contextTracker_t parentContext, kdl_tokenization_t *t, size_t *i, kdl_execute_t *out) {
    ERROR_START

    kdl_execute_t result;
    // Lol all my problems solved.
    // All zero-init.
    // Very fancy.
    memset(&result, 0, sizeof(kdl_execute_t));
    kdl_token_t token = t->tokens[*i];
    bool gotNewContext = false;
    contextTracker_t context = parentContext;

    if (token.type == KDL_TK_WORD) {
        getContextString(s, context, &result.order.context);
        result.order.verb = (char *) s.malloc(sizeof(char) * (token.valueLen + 1));
        memcpy(result.order.verb, token.value, token.valueLen);
        result.order.verb[token.valueLen] = '\0';
        // Now for the parameters
        result.order.params = (kdl_compute_t *) s.malloc(sizeof(kdl_compute_t) * MAX_COMPUTE_PARAMS);
        result.order.nParams = 0; // Formality, already zero from memset

        (*i)++;

        kdl_token_t lToken = t->tokens[*i];
        while (lToken.type != KDL_TK_CTRL || tokenEqCharNT(lToken, '(')) {
            if (result.order.nParams >= MAX_COMPUTE_PARAMS) {
                ERROR(KDL_ERR_BUF, "Compute buffer size exceeded while parsing value", lToken)
            }
            kdl_compute_t compute;
            TEST(getValue(s, context, t, i, &compute))
            result.order.params[result.order.nParams++] = compute;
            lToken = t->tokens[*i];
        }

        result.order.params = (kdl_compute_t *) s.realloc(result.order.params, sizeof(kdl_compute_t) * result.order.nParams);

    } else if (tokenEqChar22(token, ':', ':', KDL_TK_CTRL)) {
        if (gotNewContext) {
            // We have a new mark, "(xxx: ...", and immediately go to rule list. What?
            ERROR(KDL_ERR_UNX, "Unexpected '::' while parsing execute", token)
        }
    } else {
        ERROR(KDL_ERR_UNX, "Unexpected tokens while processing execute", token)
    }

    kdl_token_t token2 = t->tokens[*i];

    if (tokenEqChar22(token2, ':', ':', KDL_TK_CTRL)) {
        (*i)++;
        TEST(getProgram(s, context, t, i, &result.child, ')'))
    } else {
        kdl_token_t finalToken = t->tokens[*i];
        if (!tokenEqChar(finalToken, ')', KDL_TK_CTRL)) {
            ERROR(KDL_ERR_EXP, "Expected ')' at end of execute", finalToken)
        }
        (*i)++;
    }

    *out = result;

    ERROR_IS

    freeExecute(s, &result);

    ERROR_NONE

    if (gotNewContext) {
        freeContext(s, &context);
    }

    ERROR_END
}

kdl_error_t getRule(kdl_state_t s, contextTracker_t context, kdl_tokenization_t *t, size_t *i, kdl_rule_t *rule) {
    ERROR_START

    kdl_rule_t result;
    memset(&result, 0, sizeof(kdl_rule_t));
    result.active = false; // Formality

    if (!tokenEqChar(t->tokens[*i], '(', KDL_TK_CTRL)) {
        ERROR(KDL_ERR_EXP, "Expected '(' at start of rule", t->tokens[*i])
    }

    TEST(getCompute(s, context, t, i, &result.compute, '?'))
    TEST(getExecute(s, context, t, i, &result.execute))

    // Already checked for ')' by getExecute and children functions
    // We are one past the last character of the rule, and ready to pass control
    // to the parent

    *rule = result;

    ERROR_IS

    freeRule(s, &result);

    ERROR_NONE

    ERROR_END
}



kdl_error_t getProgram(kdl_state_t s, contextTracker_t context, kdl_tokenization_t *t, size_t *i, kdl_program_t *out, char terminate) {
    ERROR_START

    assert(t->nTokens > 0);

    kdl_program_t result;
    memset(&result, 0, sizeof(kdl_program_t));

    size_t programSize = PROGRAM_BUFFER_SIZE;
    result.rules = (kdl_rule_t *) s.malloc(sizeof(kdl_rule_t) * programSize);
    result.length = 0; // Formality, already done by memset

    kdl_token_t token = t->tokens[*i];
    while (!tokenEqChar(token, terminate, KDL_TK_CTRL)) {
        if (result.length >= programSize) {
            programSize += PROGRAM_BUFFER_SIZE;
            result.rules = (kdl_rule_t *) s.malloc(sizeof(kdl_rule_t) * programSize);
        }

        kdl_rule_t rule;
        TEST(getRule(s, context, t, i, &rule))
        result.rules[result.length++] = rule;

        if (*i >= t->nTokens) {
            if (terminate == '\0') {
                break;
            } else {
                ERROR(KDL_ERR_EOF, "Reached EOF while reading nested rule list", t->tokens[*i-1])
            }
        }
        token = t->tokens[*i];
    }

    (*i)++;

    result.rules = (kdl_rule_t *) s.realloc(result.rules, sizeof(kdl_rule_t) * result.length);

    *out = result;

    ERROR_IS

    freeProgram(s, &result);

    ERROR_NONE

    ERROR_END
}

kdl_error_t kdl_parse(kdl_state_t s, const char *input, kdl_program_t *out) {
    ERROR_START


    kdl_tokenization_t tokens;
    kdl_program_t program;
    contextTracker_t context;

    memset(&tokens, 0, sizeof(kdl_tokenization_t));
    memset(&program, 0, sizeof(kdl_program_t));
    mkContext(s, 0, &context);

    TEST(tokenize(s, input, &tokens))
    size_t i = 0;
    TEST(getProgram(s, context, &tokens, &i, &program, '\0'))

    *out = program;

    ERROR_IS

    ERROR_NONE

    freeTokenization(s, &tokens);

    ERROR_END
}

void kdl_freeProgram(kdl_state_t s, kdl_program_t *p) {
    freeProgram(s, p);
}
