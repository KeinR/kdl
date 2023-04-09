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
    PARTICLE
};

struct tokenData {
    token t;
    icu::UnicodeString value;
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

int isParticle(UChar32 f, UChar32 s) {
    // Just the ones we happen to use
    bool first = f == U'に' || f == U'で' || f == U'へ' || f == U'は';
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

bool tokenizeString(const icu::UnicodeString &input, std::vector *output) {
    icu::StringCharacterIterator iter(input);
    while (iter.hasNext()) {
        UChar32 c = iter.current32();
        UChar32 n = U'\0';
        iter.next();
        if (iter.hasNext()) {
            n = iter.current32();
        }
        iter.previous();

        icu::UnicodeString out;
        token t;

        // Reference:
        // http://www.rikai.com/library/kanjitables/kanji_codes.unicode.shtml
        // https://stackoverflow.com/a/30200250
        if (isPunctuation(c)) {
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
        } else {
            // Error: invalid characters detected!!!!
            return false;
        }
        output.push_back(tokenData{t, out});
    }
    return true;
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

