#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

void infixToPostfix(element_t *input, size_t inputLen, int maxPrec, void ***out, size_t *outLength) {

    typedef struct {
        int realPrec;
        void *data;
    } level_t;

    // Prevents precidence collisions
    const int precStride = maxPrec + 1;

    level_t *levels = (level_t *) malloc(sizeof(element_t) * ((inputLen - 1) / 2));
    void  **stack = (void **) malloc(sizeof(void *) * inputLen);
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

    free(levels);

    stack = realloc(stack, sizeof(void *) * stackLen);

    *out = stack;
    *outLength = stackLen;
}

void runTestCase(element_t *input, size_t inputLen, const char *name, const char *expect) {
    char **out = NULL;
    size_t len = 0;
    int maxPrec = 0;
    for (size_t i = 0; i < inputLen; i++) {
        if (input[i].prec > maxPrec) {
            maxPrec = input[i].prec;
        }
    }
    infixToPostfix(input, inputLen, maxPrec, (void ***) &out, &len);
    size_t eLen = strlen(expect);

    char buffer[128] = {0};
    size_t nLen = 0;
    for (size_t i = 0; i < len; i++) {
        buffer[nLen++] = ((char *)out[i])[0];
        if (i + 1 < len) {
            buffer[nLen++] = ' ';
        }
    }
    buffer[nLen] = '\0';

    printf("Testing '%s': ", name);
    if (strcmp(expect, buffer)) {
        printf("FAIL: Expect '%s', got ", expect);
    } else {
        printf("PASS: ");
    }
    printf("'%s'\n", buffer);
}

void runString(const char *value, const char *expect) {
    int map[256] = {0};
    map['-'] = 2;
    map['+'] = 2;
    map['*'] = 3;
    map['/'] = 3;
    map['^'] = 4;
    map['='] = 1;
    map['|'] = 5;
    map['&'] = 6;
    map['!'] = 7;

    element_t buffer[128];
    size_t bufferLen = 0;
    for (size_t i = 0; value[i]; i++) {
        char c = value[i];
        if (c == ' ') {
            continue;
        }
        element_t e;
        e.precChange = 0;
        e.prec = 0;
        e.data = (void *) (value + i);
        if (c == '(') {
            e.precChange = 1;
        } else if (c == ')') {
            e.precChange = -1;
        } else if (map[c]) {
            e.prec = map[c];
        }
        buffer[bufferLen++] = e;
    }

    runTestCase(buffer, bufferLen, value, expect);
}

int main() {
    // a & (b | c)
    element_t inputA[] = {
        {0, 0, "a"},
        {0, 2, "&"},
        {1, 0, "("},
        {0, 0, "b"},
        {0, 1, "|"},
        {0, 0, "c"},
        {-1, 0, ")"}
    };
    runTestCase(inputA, sizeof(inputA) / sizeof(element_t), "a & (b | c)", "a b c | &");
    runString("a & (b | c)", "a b c | &");
    runString("a & b | ! c", "a b & c ! |");
    runString("a & ! (b | ! c)", "a b c ! | ! &");
    runString("(a + b) * (c - d) = 4 / 2", "a b + c d - * 4 2 / =");


    /*

    // a & b | ! c
    element_t inputB[] = {
        {0, 0, 'a'},
        {0, 2, '&'},
        {0, 0, 'b'},
        {0, 1, '|'},
        {0, 3, '!'},
        {0, 0, 'c'}
    };
    runTestCase(inputB, sizeof(inputB) / sizeof(element_t), lmap, "a & (b | c)", "a b c | &");

    // (a + b) * (c - d)
    element_t inputC[] = {
        {1, 1, '('},
        {0, 1, 'a'},
        {0, 1, '+'},
        {0, 1, 'b'},
        {-1, 1, ')'},
        {0, 0, '*'},
        {0, 1, 'c'},
        {0, 1, '-'},
        {0, 1, 'd'}
    };
    runTestCase(inputC, sizeof(inputC) / sizeof(element_t), amap, "(a + b) * (c - d)", "a b + c d - *");

    // (a + b) * (c - d) = 4 / 2
    element_t inputD[] = {
        {0, 1, 'a'},
        {0, 1, '+'},
        {0, 1, 'b'},
        {0, 0, '*'},
        {0, 1, 'c'},
        {0, 1, '-'},
        {0, 1, 'd'},
        {0, 0, '='},
        {0, 0, '4'},
        {0, 0, '/'},
        {0, 0, '2'},
    };
    runTestCase(inputD, sizeof(inputD) / sizeof(element_t), amap, "(a + b) * (c - d) = 4 / 2", "a b + c d - * 4 2 / =");


    // a + m * x / b - q
    element_t inputE[] = {
        {0, 'a'},
        {0, '+'},
        {0, 'm'},
        {0, '*'},
        {0, 'x'},
        {0, '/'},
        {0, 'b'},
        {0, '-'},
        {0, 'q'}
    };
    runTestCase(inputE, sizeof(inputE) / sizeof(element_t), amap, "a + m * x / b - q", "a m x b / * q - +");
    */



    return 0;
}
