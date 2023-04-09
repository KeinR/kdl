#include <iostream>


typedef unsigned long codepoint_t;

unsigned char peekString(const std::string &str, int index) {
    if (index < str.size()) {
        return str[index];
    }
    return '\0';
}

bool isTrailingCodepoint(unsigned char c) {
    return (c & 0b11000000) == 0b10000000;
}

bool decodeCodepoint(const std::string &str, int index, codepoint_t *codepoint, int *length) {
    unsigned char first = peekString(str, index);
    unsigned char second = peekString(str, index+1);
    unsigned char third = peekString(str, index+2);
    unsigned char fourth = peekString(str, index+3);
    unsigned char fifth = peekString(str, index+3);
    unsigned char sixth = peekString(str, index+3);
    bool isAscii = true;
    if (first & 0b10000000 == 0) {
        *codepoint = (int) first;
        *length = 1;
        isAscii = false;
    } else if ((first & 0b11100000) == 0b11000000) {
        if (isTrailingCodepoint(second)) {
            *codepoint = ((int)((first & 0b00011111) << 6)) | ((int)(second & 0b00111111));
            *length = 2;
        }
    } else if ((first & 0b11110000) == 0b11100000) {
        if (isTrailingCodepoint(second) && isTrailingCodepoint(third)) {
            *codepoint = ((int)((first & 0b00001111) << 12)) | (((int)(second & 0b00111111)) << 6) | ((int)(third & 0b00111111));
            *length = 3;
        }
    } else if ((first & 0b11111000) == 0b11110000) {
        if (isTrailingCodepoint(second) && isTrailingCodepoint(third) && isTrailingCodepoint(fourth)) {
            *codepoint = ((int)((first & 0b00001111) << 18)) | (((int)(second & 0b00111111)) << 12) | (((int)(third & 0b00111111)) << 6) | ((int)(fourth & 0b00111111));
            *length = 4;
        }
    } else if ((first & 0b11111100) == 0b11111000) {
        // Refuse lol
        return false;
    } else if ((first & 0b11111110) == 0b11111100) {
        // Refuse lol
        return false;
    }
    // Failsafe, revert to ascii
    // Probably extended ascii in this case
    if (isAscii) {
        *codepoint = (int) first;
        *length = 1;
    }
    return true;
}


void main(int argc, char **argv) {
    // Ignore all args lol
    if (argc > 1) {
        // Undefined behavior lol
        abort();
    }
}


