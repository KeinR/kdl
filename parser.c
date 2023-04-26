#include "tokenizer.h"

#include <stdlib.h>
#include <string.h>
#include <errorno.h>

#define MAX_STRING_LEN 1028

#define TOKEN_BUFFER_SIZE 512
#define KDL_MAX_EXP_SIZE 512
#define MAX_CONTEXT_DEPTH 512
#define CONTEXT_BUFFER_SIZE MAX_STRING_LEN

#define MAX_PREC 20

#define ERROR_START kdl_error_t _err = noError();
#define ERROR_CLEAN error:
#define ERROR_LEAVE goto end;
#define ERROR_END end: return _err;
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
} contextTracker_t;

kdl_error_t mkError(int code, const char *message, const char *pointer, size_t length, bool hasLength) {
    kdl_error_t e;
    e.code = code;
    e.message = message;
    e.data = pointer;
    e.dataLen = length;
    e.hasDataLen = hasLength;
}

kdl_error_t noError() {
    kdl_error_t e;
    e.code = KDL_ERR_OK;
    e.message = "No error";
    e.token.type = -1;
    e.token.value = NULL;
    e.token.valueLen = 0;
    return e;
}

bool isError(kdl_error_t error) {
    return error.code != KDL_ERR_OK;
}

// Return is null terminated
void getContextString(kdl_state_t s, contextTracker_t tracker, char **out) {
    *out = (char *) s.malloc(sizeof(char) * (tracker.contextLen + 1));
    memcpy(*out, tracker.context, sizeof(char) * tracker.contextLen);
    (*out)[tracker.contextLen] = '\0';
}

bool tokenEqChar2NT(kdl_token_t t, char c) {
    return t.valueLen == 2 && token.value[1] == c;
}

bool tokenEqCharNT(kdl_token_t t, char c) {
    return t.valueLen == 1 && token.value[0] == c;
}

bool tokenEqChar(kdl_token_t t, char c, int type) {
    return t.type == type && t.valueLen == 1 && t.value[0] == c;
}

void addToContext(contextTracker_t *tracker, const char *word, size_t wordLen) {
    if (tracker->indicesLen + 1 > CONTEXT_BUFFER_SIZE) {
        // Error: buffer size exceeded
    }
    if (tracker->contextLen + wordLen + 1 > CONTEXT_BUFFER_SIZE) {
        // Error: buffer size exceeded
    }
    tracker->content[tracker->contextLen] = ' ';
    memcpy(tracker->context + tracker->contextLen + 1, word, wordLen);
    tracker->indices[tracker->indicesLen++] = tracker->contextLen;
    tracker->contextLen += wordLen + 1;
}

void mkContext(kdl_state_t s, contextTracker_t *out) {
    out->context = (char *) s.malloc(sizeof(char) * CONTEXT_BUFFER_SIZE);
    out->indices = (size_t *) s.malloc(sizeof(size_t) * CONTEXT_BUFFER_SIZE);
    out->contextLen = 0;
    out->indicesLen = 0;
}

void freeContext(kdl_state_t s, contextTracker *tracker) {
    s.free(out->context);
    s.free(out->indices);
    out->context = NULL;
    out->indices = NULL;
    out->contextLen = 0;
    out->indicesLen = 0;
}

void getContext(kdl_state_t s, contextTracker_t base, int levels, contextTracker_t *out) {
    mkContext(s, out);
    if (levels < base.indicesLen) {
        addToContext(out, base.context, levels > 0 ? base.indices[base.indicesLen - levels] : base.contextLen);
    }
}

kdl_error_t getToken(const char *input, kdl_token_t *token, bool *eof) {
    ERROR_START

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
    } else {
        *eof = true;
        ERROR_LEAVE
    }

    switch (c) {
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
    case '[':
        t =  KDL_TK_STR;
        i++;
        for (; input[i+l] != ']'; l++) {
            if (!input[i+l]) {
                NTERROR(KDL_ERR_UNX, "Expected something other than EOF while reading string literal", input)
            }
        }
        l--;
        break;
    case '{':
        t =  KDL_TK_VAR;
        i++;
        for (; input[i+l] != '}'; l++) {
            if (!input[i+l]) {
                NTERROR(KDL_ERR_UNX, "Expected something other than EOF while reading variable", input)
            }
        }
        l--;
        break;
    default:
        if (c >= '0' && c <= '9') {
            t = KDL_TK_INT;
            for (; input[i+l] >= '0' && input[i+l] <= '9'; l++);
            if (input[i] == '.') {
                t = KDL_TK_FLOAT;
                for (; input[i+l] >= '0' && input[i+l] <= '9'; l++);
            }
            if (input[i] == '%') {
                t = KDL_TK_PERC;
                l++;
            }
        } else {
            t = KDL_TK_WORD;
            for (;  (input[i+l] >= 'a' && input[i+l] <= 'a') ||
                    (input[i+l] >= 'A' && input[i+l] <= 'Z') ||
                    input[i+l] == '_'
                    ; l++);
        }
    }
    token->value = input + i;
    token->valueLen = l;
    token->type = t;

    ERROR_CLEAN

    ERROR_END
}

void createStringCopyNoWhitespace(kdl_state_t s, const char *input, size_t length, char **out) {
    char *result = (char *) s.malloc(sizeof(char) * (length + 1));
    size_t resultLen = 0;
    for (size_t i = 0; i < length; i++) {
        // Very lazy but very fast
        if (input[i] > 0x20) {
            result[resultLen++] = input[i];
        } else if (resultLen > 0 && result[resultLen - 1] > 0x20) {
            // Admitidly this also means we admit funny characters.
            // This also means that we admit funny characters.
            result[resultLen++] = ' ';
        }
    }
    result[resultLen] = '\0';
    result = (char *) s.realloc(result, sizeof(char) * (resultLen + 1));

    *out = result;
}


void infixToPostfix(kdl_state_t s, element_t *input, size_t inputLen, int maxPrec, void ***out, size_t *outLength) {

    typedef struct {
        int realPrec;
        void *data;
    } level_t;

    // Prevents precidence collisions
    const int precStride = maxPrec + 1;

    level_t *levels = (level_t *) s.malloc(sizeof(element_t) * ((inputLen - 1) / 2));
    void  **stack = (void **) s.malloc(sizeof(void *) * inputLen);
    size_t levelsLen = 0;
    size_t stackLen = 0;

    int currentPrec = 0;
    for (size_t i = 0; i < inputLen; i++) {
        element_t e = input[i];
        assert(e.prec <= maxPrec);
        if (e.precChange != 0) {
            currentPrec += e.precChange * precStride;
        } else if (e.prec > 0) {
            int realPrec = e.prec + currentPrec;

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



kdl_error_t kdl_tokenize(kdl_state_t s, const char *input, kdl_tokenization_t *out) {
    ERROR_START

    size_t size = TOKEN_BUFFER_SIZE;
    kdl_tokenization_t result;
    result.nTokens = 0;
    result.tokens = (kdl_token_t *) s.malloc(sizeof(kdl_token_t) * size);
    bool looping = true;
    kdl_token_t token;
    const char *offset = NULL;
    bool eof = false;
    while (true) {
        TEST(getToken(s, input + offset, &token, &eof))
        if (eof) {
            break;
        }
        if (result.nTokens + 1 > size) {
            size += TOKEN_BUFFER_SIZE;
            result.tokens = (kdl_token_t *) s.realloc(result.tokens, sizeof(kdl_token_t) * size);
        }
        result.tokens[result.nTokens] = token;
        result.nTokens++;
        offset = token.value + token.valueLen;
    }

    result.tokens = (kdl_token_t *) s.realloc(result.tokens, sizeof(kdl_token_t) * result.nTokens);

    *out = result;

    ERROR_CLEAN

    s.free(result.tokens);

    ERROR_END
}

void append(kdl_state_t s, void **dest, size_t *length, size_t *size, void *source, size_t bytes) {
    if (*length + 1 > *size) {
        *size += 512;
        *dest = s.realloc(*dest, bytes * (*size));
    }
    memcpy(*dest + *length, source, bytes);
    (*length)++;
}

void getString(kdl_state_t s, kdl_tokenization_t *t, size_t *i, char **out) {
    char *result = (char *) s.malloc(sizeof(char) * MAX_STRING_LEN);
    size_t resultLen = 0;
    size_t f = *i;
    for (; f < t->nTokens && t->tokens[f].type == KDL_TK_WORD; i++) {
        size_t len = t->tokens[f].valueLen;
        if (len + 1 > MAX_STRING_LEN) {
            // Error: max string length exceeded
            assert(false);
        }
        memcpy(result + resultLen, t->tokens[f].value, sizeof(char) * len);
        result[resultlen + len] = ' ';
        resultLen += len + 1;
    }
    result[resultLen] = '\0';
    if (resultLen > 0) {
        *out = result;
        *i += f;
    }
}

bool endInput(kdl_tokenization_t *t, size_t i) {
    return i >= t->nTokens;
}

kdl_error_t getGet(kdl_state_t s, contextTracker_t parent, kdl_tokenization_t *t, size_t *i, kdl_get_t *get, char terminate, char climber, char digger) {
    ERROR_START

    kdl_op_t **ops = (kdl_op_t **) s.malloc(sizeof(kdl_op_t) * KDL_MAX_GET_LEN);
    size_t opsLen = 0;
    contextTracker_t *contexts = (contextTracker_t *) s.malloc(sizeof(contextTracker_t) * MAX_CONTEXT_DEPTH);
    contexts[0] = parent;
    size_t contextsLen = 1;
    element_t *elements = (element_t *) s.malloc(sizeof(element_t) * KDL_MAX_GET_LEN);
    size_t elementsLen = 0;

    int dug = 0;

    // Loop variables
    char *tokenString = NULL;
    void *value = NULL;

    for (; *i < t->nTokens && (t->tokens[*i].value[0] != terminate || dug > 0); (*i)++) {
        if (opsLen >= KDL_MAX_GET_LEN) {
            // Error: GET max length exceeded
        }
        token_t token = t->tokens[*i];
        if (token.value[0] == digger) {
            dug++;
        }
        if (token.value[0] == climber) {
            dug--;
        }
        if (token.value[0] == terminate && dug <= 0) {
            // End of statement
            break;
        }

        tokenString = NULL;
        createStringCopyNoWhitespace(s, token.value, token.valueLen, &tokenString);
        value = NULL;

        element_t e;
        e.prec = 0;
        e.precChange = 0;
        e.data = NULL;
        int op = KDL_OP_NOOP;
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
                    e.prec = 4;
                    break;
                case '/':
                    op = KDL_OP_DIV;
                    e.prec = 5;
                    break;
                case '*':
                    op = KDL_OP_MUL;
                    e.prec = 5;
                    break;
                case '=':
                    op = KDL_OP_EQU;
                    e.prec = 3;
                    break;
                case '<':
                    op = KDL_OP_LTH;
                    e.prec = 3;
                case '>':
                    op = KDL_OP_GTH;
                    e.prec = 3;
                case '&':
                    op = KDL_OP_AND;
                    e.prec = 2;
                    break;
                case '|':
                    op = KDL_OP_OR;
                    e.prec = 1;
                    break;
                case '!':
                    op = KDL_OP_NOT;
                    e.prec = 6;
                    break;
                case '(':
                    e.precChange = 1;
                    if (*i + 2 < t->nTokens &&
                            (tokenEqChar(t->tokens[*i + 1], '^', KDL_TK_CTRL) ||
                             (t->tokens[*i + 1].type == KDL_TK_WORD &&
                              (t->tokens[*i + 2].type == KDL_TK_WORD ||
                               (tokenEqChar(t->tokens[*i + 2], ':', KDL_TK_CTRL)))))) {
                        size_t f = *i;
                        int jumpers = 0;
                        while (tokenEqChar(t->tokens[*i], '^', KDL_TK_CTRL)) {
                            jumpers++;
                            (*i)++;
                            if (*i >= t->nTokens) {
                                ERROR(KDL_ERR_EOF, "Reached EOF while reading mark jumpers", t->tokens[*(i-1)])
                            }
                        }
                        contextTracker_t nc;
                        if (jumpers == 0) {
                            mkContext(s, &nc);
                        } else {
                            getContext(s, contexts[contextLen - 1], jumpers - 1, &nc);
                        }
                        while (t->tokens[*i].type == KDL_TK_WORD) {
                            addToContext(&nc);
                            (*i)++;
                            if (*i >= t->nTokens) {
                                ERROR(KDL_ERR_EOF, "Reached EOF while reading mark", t->tokens[*(i-1)])
                            }
                        }
                        if (!tokenEqChar(t->tokens[*i], ':', KDL_TK_CTRL)) {
                            ERROR(KDL_ERR_EXP, "Expected ':' at end of mark", t->tokens[*i])
                        }
                        contexts[contextLen++] = nc;
                    }
                    break;
                case ')':
                    e.precChange = -1;
                    if (contextsLen <= 1) {
                        ERROR(KDL_ERR_UNX, "Unmatched ')'", t->tokens[*i])
                    }
                    freeContext(&contexts[--contextsLen]);
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
        case KDL_TK_WORD:
            // Variable, by process of elimination
            op = KDL_OP_PVAR;
            break;
        case KDL_TK_INT:
            value = malloc(sizeof(kdl_int_t));
            *((kdl_int_t *)value) = (kdl_int_t) strtoll(tokenString, NULL, 10);
            if (errno == ERANGE) {
                ERROR(KDL_ERR_VAL, "Integer too large", token)
            }
            op = KDL_OP_PINT;
            break;
        case KDL_TK_FLOAT:
            value = malloc(sizeof(kdl_float_t));
            *((kdl_float_t *) value) = (kdl_float_t) strtold(tokenString, NULL);
            if (errno == ERANGE) {
                ERROR(KDL_ERR_VAL, "Floating point number too large", token)
            }
            op = KDL_OP_PFLOAT;
            break;
        case KDL_TK_PERC:
            value = malloc(sizeof(kdl_float_t));
            // Docs say that strtold() doesn't care if there's junk at the end
            // of the input.
            // And I suppose that goes for our trailing %
            *((kdl_float_t *) value) = (kdl_float_t) strold(tokenString, NULL);
            if (errno == ERANGE) {
                ERROR(KDL_ERR_VAL, "Percentage value too large", token)
            }
            op = KDL_OP_PPERC;
            break;
        case KDL_TK_VAR:
            op = KDL_OP_PVAR;
            break;
        case KDL_TK_STR:
            op = KDL_OP_PSTR;
            break;
        default:
            assert(false);
        }
        if (op != KDL_OP_NOOP) {
            kdl_op_t *opv = NULL;
            opv = (kdl_op_t *) malloc(sizeof(kdl_op_t));
            kdl_token_t token = t->tokens[*i];
            if (value == NULL) {
                value = tokenString;
            }
            opv->op = op;
            opv->value = value;
            getContextString(s, contexts[contextLen - 1], &opv->context);
            e.data = opv;
        } else {
            s.free(tokenString);
        }
        elements[elementsLen++] = e;
        assert(prec <= MAX_PREC);
    }

    kdl_op_t **stack = NULL;
    size_t stackLen = 0;
    infixToPostfix(elements, elementsLen, MAX_PREC, &stack, &stackLen);

    // Flatten the stack.
    // Supposidly this improves CPU cache or smthn idk.
    // Well I like flat arrays so...!!!!
    compute_t compute;
    compute.opers = (kdl_op_t *) s.malloc(sizeof(kdl_op_t) * stackLen);

    for (size_t f = 0; f < stackLen; f++) {
        compute.opers[f] = *(stack[f]);
    }

    *get = compute;

    ERROR_CLEAN

    for (size_t f = 0; f < contextsLen; f++) {
        freeContext(s, contexts[f]);
    }

    for (size_t f = 0; f < stackLen; f++) {
        s.free(stack[f]);
    }

    s.free(contexts);
    s.free(elements);
    s.free(ops);
    s.free(stack);

    ERROR_END
}

kdl_error_t kdl_build(kdl_state_t s, kdl_tokeniziation_t *t, size_t *i, size_t endToken, kdl_program_t *out) {
    ERROR_START

    kdl_program_t program;
    size_t programSize = 512;
    program.length = 0;
    program.rules = (kdl_rule_t *) s.malloc(sizeof(kdl_rule_t) * programSize);

    while (*i < t->nTokens) {
        if (t->tokens[*i].value[0] != '(') {
            ERROR(KDL_ERR_EXP, "Expected '(' at start of statement", t->tokens[*i])
        }
        kdl_rule_t rule;
        error = getGet(s, t, i, &rule.get, '?', '\0', '\0');
        TEST
        error = getSet(s, t, i, &rule.set);
        TEST
        append(s, &program.rules, &program.length, &programSize, &rule, sizeof(kdl_rule_t));
        assert(tokenEqChar(t->tokens[*i], ')', KDL_TK_CTRL));
    }

    program.rules = (kdl_rule_t *) s.realloc(program.rules, sizeof(kdl_rule_t) * program.length);
    *out = program;

    ERROR_CLEAN

    free(program.rules);

    ERROR_END
}
