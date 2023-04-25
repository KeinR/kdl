#include <stdlib.h>
#include <assert.h>
#include <sys/param.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    int depth;
    // NULL if data object
    char op;
} depth_t;

void infixToPostfix(depth_t *input, size_t inputLen, int *depthMap, char **out, size_t *outLength) {

    int max = 0;
    for (int i = 0; i < 256; i++) {
        assert(depthMap[i] >= 0);
        if (depthMap[i] > max) {
            max = depthMap[i];
        }
    }

    // Prevents precidence collisions
    const int stride = max + 1;

    depth_t *levels = (depth_t *) malloc(sizeof(depth_t) * ((inputLen - 1) / 2));
    char *stack = (char *) malloc(sizeof(char *) * inputLen);
    size_t levelsLen = 0;
    size_t stackLen = 0;

    assert(inputLen % 2 == 1);

    int prevRealDepth = -1;
    int prevOp = '\0';
    for (size_t i = 0; i < inputLen; i += 2) {
        int depth = input[i].depth;
        int nextOp = '\0';
        int nextDepth = -1;
        if (i + 1 < inputLen) {
            nextOp = input[i+1].op;
            nextDepth = input[i+1].depth;
        }
        int nextRealDepth = depthMap[nextOp] + nextDepth * stride;

        stack[stackLen++] = input[i].op;

        while (levelsLen > 0 && nextRealDepth < levels[levelsLen - 1].depth) {
            depth_t l = levels[levelsLen - 1];
            stack[stackLen++] = l.op;
            levelsLen--;
        }

        depth_t l;
        l.op = nextOp;
        l.depth = nextRealDepth;
        levels[levelsLen++] = l;

        prevRealDepth = nextRealDepth;
        prevOp = nextOp;

        assert(levelsLen <= (inputLen - 1) / 2);
    }

    free(levels);

    assert(stackLen == inputLen);

    *out = stack;
    *outLength = stackLen;
}

void runTestCase(depth_t *input, size_t inputLen, int *map, const char *name, const char *expect) {
    char *out = NULL;
    size_t len = 0;
    infixToPostfix(input, inputLen, map, &out, &len);
    size_t eLen = strlen(expect);

    char buffer[128] = {0};
    size_t nLen = 0;
    for (size_t i = 0; i < len; i++) {
        buffer[nLen++] = out[i];
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

int main() {
    int lmap[256];
    memset(lmap, 0, sizeof(int) * 256);
    lmap['&'] = 2;
    lmap['|'] = 1;

    // a & b | c
    depth_t inputA[] = {
        {0, 'a'},
        {0, '&'},
        {0, 'b'},
        {0, '|'},
        {0, 'c'}
    };
    runTestCase(inputA, sizeof(inputA) / sizeof(depth_t), lmap, "a & b | c", "a b & c |");

    // a & (b | c)
    depth_t inputB[] = {
        {0, 'a'},
        {0, '&'},
        {9, 'b'},
        {9, '|'},
        {9, 'c'}
    };
    runTestCase(inputB, sizeof(inputB) / sizeof(depth_t), lmap, "a & (b | c)", "a b c | &");

    int amap[256];
    memset(amap, 0, sizeof(int) * 256);
    amap['+'] = 2;
    amap['-'] = 2;
    amap['/'] = 3;
    amap['*'] = 3;
    amap['^'] = 4;
    amap['='] = 1;

    // (a + b) * (c - d)
    depth_t inputC[] = {
        {1, 'a'},
        {1, '+'},
        {1, 'b'},
        {0, '*'},
        {1, 'c'},
        {1, '-'},
        {1, 'd'}
    };
    runTestCase(inputC, sizeof(inputC) / sizeof(depth_t), amap, "(a + b) * (c - d)", "a b + c d - *");

    // (a + b) * (c - d) = 4 / 2
    depth_t inputD[] = {
        {1, 'a'},
        {1, '+'},
        {1, 'b'},
        {0, '*'},
        {1, 'c'},
        {1, '-'},
        {1, 'd'},
        {0, '='},
        {0, '4'},
        {0, '/'},
        {0, '2'},
    };
    runTestCase(inputD, sizeof(inputD) / sizeof(depth_t), amap, "(a + b) * (c - d) = 4 / 2", "a b + c d - * 4 2 / =");


    // a + m * x / b - q
    depth_t inputE[] = {
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
    runTestCase(inputE, sizeof(inputE) / sizeof(depth_t), amap, "a + m * x / b - q", "a m x b / * q - +");





    return 0;
}
