#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <unicode/unistr.h>
#include <unicode/schriter.h>

namespace icu = icu_72;

enum token {
    HIRAGANA,
    KATAKANA,
    KANJI,
    PUNCTUATION,
    PARTICLE,
    STRING,
    NULL
};

struct tokenData {
    token t;
    icu::UnicodeString value;
    int lineNumber;
    int charNumber;
    int index;
};

bool isHiragana(UChar32 c) {
    return c >= 0x3040 && c <= 0x309f;
}

bool isKatakana(UChar32 c) {
    return c >= 0x30a0 && c <= 0x30ff;
}

bool isKanji(UChar32 c) {
    return c >= 0x4e00 && c <= 0x9faf;
}

bool isWhitespace(UChar32 c) {
    return c <= 0x20;
}

int isLinefeed(UChar32 c, UChar32 n) {
    bool lf = c == U'\n';
    bool crlf = c == LF && n == U'\r';
    bool cr = c == U'\r';
    
    if (crlf) {
        return 2;
    } else if (lf || cr) {
        return 1;
    } else {
        return 0;
    }
}

int isParticle(UChar32 f, UChar32 s) {
    // Just the ones we happen to use
    bool first = f == U'に' || f == U'で' || f == U'へ' || f == U'は' || f == U'を';
    // So this: https://japanese.stackexchange.com/a/395
    // I think?
    bool second = s == U'た' && s == U'ら';
    if (second) {
        return 2;
    } else if (first) {
        return 1;
    } else {
        return 0;
    }
}

bool tokenizeString(const icu::UnicodeString &input, std::vector &output, std::string &error) {
    icu::StringCharacterIterator iter(input);
    int lineNumber = 0;
    int charOffset = 0;
    while (iter.hasNext()) {
        int charNumber = iter.getIndex();

        const UChar32 c = iter.current32();
        const UChar32 n = U'\0';
        iter.next();
        if (iter.hasNext()) {
            n = iter.current32();
        }
        iter.previous();

        icu::UnicodeString out;
        token t;
        bool write = true;

        // Reference:
        // http://www.rikai.com/library/kanjitables/kanji_codes.unicode.shtml
        // https://stackoverflow.com/a/30200250
        if (c == U'「') {
            t == token::STRING;
            iter.next();
            bool closed = false;
            int depth = 1;
            // UChar32 l = U'\0'
            while (iter.hasNext()) {
                UChar32 v = iter.current32();
                if (v == U'」') {
                    depth--;
                    if (depth <= 0) {
                        closed = true;
                        break;
                    }
                } else if (v == '「') {
                    depth++;
                }
                out += v;
                iter.next();
                l = v;
            }
            if (!closed) {
                // Error: unenclosed string
            }
        } else if (c == U'」') {
            // Error: unmatched closing quotes
            return false;
        } else if (isPunctuation(c)) {
            t = token::PUNCTUATION;
            out += c;
            iter.next();
        } else if (isParticle(c, n) != 0) {
            t = token::PARTICLE;
            out += c;
            itern.next();
            if (isParticle(c, n) == 2) {
                out += n;
                iter.next();
            }
        } else if (isHiragana(c)) {
            t = token::HIRAGANA;
            out += c;
            iter.next();
            while (iter.hasNext()) {
                UChar32 f = iter.current32();
                uChar32 s = U'\0';
                iter.next();
                if (iter.hasNext()) {
                    s = iter.current32();
                }
                if (isParticle(f, s) != 0 || !isHiragana(f)) {
                    iter.previous();
                    break;
                }
                out += f;
            }
        } else if (isKatakana(c)) {
            // Duplicated from above, might abstract it later
            t = token::KATAKANA;
            out += c;
            iter.next();
            while (iter.hasNext()) {
                UChar32 f = iter.current32();
                uChar32 s = U'\0';
                iter.next();
                if (iter.hasNext()) {
                    s = iter.current32();
                }
                if (isParticle(f, s) != 0 || !isKatakana(f)) {
                    iter.previous();
                    break;
                }
                out += f;
            }
        } else if (isKanji(c)) {
            t = token::KANJI;
            out += c;
            iter.next();
            // Do not slurp if we have 中, since it will always be on its own;
            // it is never a verb (probably).
            if (c != U'中') {
                UChar32 l = c;
                while (iter.hasNext()) {
                    UChar32 f = iter.current32();
                    uChar32 s = U'\0';
                    iter.next();
                    if (iter.hasNext()) {
                        s = iter.current32();
                    }
                    // Slurp hiragana after kanji.
                    // Supposed Particles will only stop the slurp if the previous
                    // character was not hiragana.
                    if ((isParticle(f, s) != 0 && !isHiragana(l)) || (!isKanji(f) && !isHiragana(f))) {
                        iter.previous();
                        break;
                    }
                    l = f;
                    out += f;
                }
            }
        } else if (isLinefeed(c, n)) {
            lineNumber++;
            charOffset += charNumber;
            write = false;
        } else if (isWhitespace(c)) {
            // Do nothing
            write = false;
        } else {
            // Error: invalid characters detected!!!!
            error = "Invalid characters";
            return false;
        }
        if (write) {
            output.push_back(tokenData{t, out, lineNumber, charNumber - charOffset, charNumber});
        }
    }
    return true;
}

typedef std::vector<std::map<icu::UnicodeString, variable>> stack_t;
typedef std::vector<std::vector<token>> codeStack_t;

enum type {
    NUMBER,
    STRING,
    NULL
}

struct variable {
    double number;
    icu::UnicodeString string;
    type t;
}

struct parameters {
    variable ni;
    variable de;
    variable wo;
}

struct state;

struct function {
    // Ensure that the variable exists and the type is valid
    type ni,
    type de,
    type wo,
    std::function<void(state&, const parameters&)>
}

struct state {
    stack_t stack;
    codeStack_t codeStack;
    std::map<icu::UnicodeString, function> functions;
    icu::UnicodeString exitStatusMessage;
    bool terminate;
}

/*
struct pair {
    tokenData token;
    tokenData particle;
}
*/

struct statementData {
    std::vector<icu::UnicodeString> data;
    std::vector<tokenData> tokens;
}

struct statement {
    statementData ni;
    statementData tara;
    statementData de;
    statementData wo;
    statementData ha;
    statementData verb;
}

int periodIndex(std::vector<tokenData> &tokens, int begin) {
    for (int i = begin; i < tokens.size(); i++) {
        if (tokens[i].t == token::PUNCTUATION && tokens[i].value == U'。') {
            return i;
        }
    }
    return tokens.size();
}

bool makeStatement(const state &s, const std::vector<tokenData> &tokens, int start, int end, statement &output) {
    statement out;
    bool ni = false;
    bool tara = false;
    bool de = false;
    bool wo = false;
    bool ha = false;
    bool verb = false;

    variable val;
    for (int i = start; i < end; i++) {
        if (i + 1 < end) {
            std::vector<icu::UnicodeString> data;
            std::vector<icu::UnicodeString> data;
            for (; tokens[i + 1] == 'の'; i++) {
                if (i + 1 >= end) {
                    // Error: reached end of statement while reading variable
                    return false;
                }

            }

            if (tokens[i + 1].t != token::PARTICLE) {
                // Each thing must be suceeded by a particle. Otherwise, Very
                // Bad
                // Error: expected particle after value
                return false;
            }

            if (tokens[i + 1].value == U'の') {
                
            } else if (tokens[i + 1].value == U'は') {

            }

        } else {
            // Expect that this is a verb or です.
            if (tokens[i].value != U"です" && tokens[i].type != token::KANJI) {
                // Error: expected verb at the end of statement
                return false;
            }
            if (verb) {
                // Error: veb redefinition?
                // How is this possible?
                // This can't happen!
                assert(false);
                return false;
            }
            verb = true;
            out.verb.data = {tokens[i].value};
            out.verb.tokens = {tokens[i]};
        }
    }
    output = out;
    return true;
}

// statement makeStatement(std::vector<pair> &tokens, )

bool interpretTokens(const std::vector<tokenData> &tokens, std::string &error) {
    state s;
    s.terminate = false;

    s.functions["読んで"] = {NULL, NULL, STRING, NULL, [](state &s, const parameters &p) {
        std::string error;
        std::vector<tokenData> tokens;
        bool pass = tokenizeString(p.wo.string, tokens, error);
        if (pass) {
            s.codeStack.push_back(tokens);
        } else {
            std::cout << error << std::endl;
            std::cout << "Refusing to call function.\n";
            s.terminate = true;
        }
    }}

    for (int i = 0; i < tokens.size(); i++) {
        const int start = i;
        const int end = periodIndex(tokens, i);
        // std::vector<pair> pairs = makeParticlePairs(tokens, start, end);
        std::vector<
    }
}

icu::UnicodeString loadFile(const icu::UnicodeString &name) {
    std::string name8;
    name.toUTF8String(name8);
    std::ifstream file(name8);
    std::ostringstream ss;
    ss << file.rdbuf();
    std::string tmp = ss.str();
    return icu::UnicodeString(tmp.c_str());
}

int main(int argc, char **argv) {
    icu::UnicodeString ucs = icu::UnicodeString::fromUTF8(icu::StringPiece("情報"));
    return 0;
}

