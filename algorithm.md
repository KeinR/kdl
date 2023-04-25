
struct level {
    int instances;
    char operator;
    int precidence;
}

struct val {
 char char;
 int precidence;
}

// Will have one dummy value at the end of the array, the EOF
input = [val array]

stack = [stack]
levels = [level stack]
precidence = [int map of 256]


for c = next(input) & advance(input):
    p = prev(input);
    n = next(input);
    advance(input);
    assert(prev and next exist in precidence map, or are out of bounds)
    int depth = max(pMatch(p.precidence, c.precidence, precidence[p.char]), pMatch(n.precidence, c.precidence, precidence[n.char]));
    if (depth == -1) {
        // There is no valid adjacent operators
        // Must be a case of (((((((((((((this)))))))))))))
        // top precidence will be -1 if it is the base level (for safety)
        depth = levels.top().precidence == -1 ? 0 : levels.top().precidence;
    }
    while (depth < levels.top().precidence) {
        for (int i = 0; i < levels.top().instances; i++) {
            stack.push(levels.top().operator);
        }
        levels.pop();
    }
    if (c.char != EOF) {
        stack.push(c.char);
    }

// There should only be one left?
for (int i = 0; i < levels.top().instances; i++) {
    stack.push(levels.top().operator);
}

