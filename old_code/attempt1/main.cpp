#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <unicode/unistr.h>
#include <unicode/schriter.h>
#include <cassert>

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
    STRING
};

struct variable {
    double number;
    icu::UnicodeString string;
    type t;
};

struct parameters {
    variable ni;
    variable de;
    variable wo;
    variable e;
    variable wa;
};

struct state;

struct function {
    // Ensure that the variable exists and the type is valid
    type ni,
    type de,
    type wo,
    type e;
    std::function<void(state&, const parameters&)> action;
};

struct adjective {
    std::function<bool(state&, const variable&)> action;
};

struct state {
    stack_t stack;
    codeStack_t codeStack;
    std::map<icu::UnicodeString, function> functions;
    std::map<icu::UnicodeString, adjetive> adjective;
    icu::UnicodeString exitStatusMessage;
    bool terminate;
};

struct statementData {
    std::vector<tokenData> tokens;
};

struct statement {
    statementData ni;
    statementData tara;
    statementData de;
    statementData wo;
    statementData wa;
    statementData e;
    statementData verb;
};

struct procStatement {
    std::vector<variable> ni;
    std::vector<variable> tara;
    std::vector<variable> de;
    std::vector<variable> wo;
    std::vector<variable> wa;
    std::vector<variable> e;
    std::vector<variable> verb;
};

int periodIndex(std::vector<tokenData> &tokens, int begin) {
    for (int i = begin; i < tokens.size(); i++) {
        if (tokens[i].t == token::PUNCTUATION && tokens[i].value == U'。') {
            return i;
        }
    }
    return tokens.size();
}

bool trySetAttribute(statementData &output, const statementData &value) {
    if (output.tokens.size() == 0) {
        output = value;
        return true;
    }
    return false;
}

bool makeStatement(const std::vector<tokenData> &tokens, const int start, const int end, statement &output) {
    statement out;

    variable val;
    for (int i = start; i < end; i++) {
        if (i + 1 < end) {
            statementData data;
            std::vector<tokenData> data;
            for (; i + 1 < end && tokens[i + 1].value == 'の'; i++) {
                data.tokens.push_back(tokens[i]);
            }
            if (i + 1 >= end) {
                // Error: reached end of statement while reading variable.
                // This happened while reading the varialbe with the の
                // A statement must be terminated by a verb.
                return false;
            }
            data.tokens.push_back(tokens[i]);

            if (tokens[i + 1].t != token::PARTICLE) {
                // Each thing must be suceeded by a particle. Otherwise, Very
                // Bad
                // Error: expected particle after value
                return false;
            }

            if (tokens[i + 1].value == U'は') {
                if (!trySetAttribute(out.wa, data)) {
                    // Error: redefinition of は
                    return false;
                }
            } else if (tokens[i + 1].value == U'で') {
                if (!trySetAttribute(out.de, data)) {
                    // Error: redefinition of で
                    return false;
                }
            } else if (tokens[i + 1].value == U'に') {
                if (!trySetAttribute(out.ni, data)) {
                    // Error: redefinition of に
                    return false;
                }
            } else if (tokens[i + 1].value == U'へ') {
                if (!trySetAttribute(out.e, data)) {
                    // Error: redefinition of へ
                    return false;
                }
            } else if (tokens[i + 1].value == U'を') {
                if (!trySetAttribute(out.wo, data)) {
                    // Error: redefinition of を
                    return false;
                }
            } else if (tokens[i + 1].value == U'た' && i + 2 < end && tokens[i + 2] == U'ら') {
                if (!trySetAttribute(out.tara, data)) {
                    // Error: redefinition of は
                    return false;
                }
                i++;
            }
            i++;

        } else {
            // Expect that this is a verb or です.
            if (tokens[i].value != U"です" && tokens[i].type != token::KANJI) {
                // Error: expected verb at the end of statement
                return false;
            }
            if (verb) {
                // Error: verb redefinition?
                // How is this possible?
                // This can't happen!
                // We're at the end!?!?
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

bool unrollParameterSingle(state &s const statementData &d, icu::UnicodeString &v) {
    icu::UnicodeString name = d.tokens[0].value;
    for (int i = 1; i < d.tokens.size(); i++) {
        icu::UnicodeString val = d.tokens[i].value;
        if (val == U"中") {
            if (state.symbolTable.count(val) == 0) {
                // Error: variable does not exist.
                return false;
            }
            name = state.symbolTable[]
        } else {
            // User is not allowed to use $
            name = name + "$" + val;
        }
    }
    v 
}

bool unrollParameters(, parameters &p) {
    unrollParameterSingle(, p.e)
}

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
            std::cout << "Refusing to call function." << std::endl;
            s.terminate = true;
        }
    }};

    s.adjectives["ぜろ"] = {[](state &s, const variable &p) -> bool {
        return p.type == type::NUMBER && p.number == 0;
    }};

    for (int i = 0; i < tokens.size();) {
        const int start = i;
        const int end = periodIndex(tokens, i);
        statement st;
        bool succeed = makeStatement(tokens, start, end, st);
        if (statement.verb.tokens.size() == 0) {
            // Error: statement requires verb.
            return false;
        }
        if (statement.verb.tokens.size() != 1) {
            // Error: too many verbs given.
            // This should be impossible.
            assert(false);
            return false;
        }
        icu::UnicodeString verbName = st.verb.tokens[0].value;
        if (st.functions.count(verbName) == 0) {
            // Error: Unknown verb
            return false;
        }
        if (verbName == U"です" &&
                st.e.size() != 0    ||
                st.wa.size() != 0   ||
                st.wo.size() != 0   ||
                st.de.size() != 0   ||
                st.tara.size() != 0 ||
                st.ni.size() != 0   ||
           ) {
            // Error: です, being a literal assignment, expects zero particlest.
            return false;
        }
        if (st.tara.tokens.size() != 0 && s.wa.tokens.size() == 0) {
            // Error: たら expects は, since it is the conditional, and は
            // specifies the comparison.
            return false;
        }
        function f = functions[verbName];
        if (f.e == type::NULL && st.e.tokens.size() != 0) {
            // Error: function does not take へ parameters
            return false;
        }
        if (f.wo == type::NULL && st.wo.tokens.size() != 0) {
            // Error: function does not take を parameters
            return false;
        }
        if (f.de == type::NULL && st.de.tokens.size() != 0) {
            // Error: function does not take で parameters
            return false;
        }
        if (f.ni == type::NULL && st.ni.tokens.size() != 0) {
            // Error: function does not take に parameters
            return false;
        }
        // Unroll parameters
        parameters p;
        bool unrollSuccess = unrollParameters(st, p);

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

