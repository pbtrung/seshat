#include "zbase32/zbase32.h"
#include <stdio.h>

static const char *alphabet = "ybndrfg8ejkmcpqxot1uwisza345h769";

/* The base32 values of each ASCII character. -1 = invalid */
static const char encoded_character_lookup[] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 18, -1, 25, 26, 27, 30, 29, 7,
    31, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, 24, 1,  12, 3,  8,  5,  6,  28, 21, 9,  10, -1, 11, 2,  16, 13, 14,
    4,  22, 17, 19, -1, 20, 15, 0,  23, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1
};

int zbase32_decode(unsigned char *decoded, const unsigned char *encoded,
                   unsigned int bits) {
    unsigned char decoded_value, *output = decoded;
    const unsigned char *input = encoded;
    unsigned int bits_to_read, undecoded_bits = bits, working_bits = 0,
                               working_bit_count = 0;

    while (undecoded_bits > 0) {
        if (working_bit_count >= 8) {
            /* There are enough decoded bits to produce a byte */
            working_bit_count -= 8;
            undecoded_bits -= 8;
            *output = (unsigned char)(working_bits >> working_bit_count);
            output++;
        } else if (working_bit_count == undecoded_bits) {
            /* There are no more encoded bits to be read */
            working_bits <<= 8 - undecoded_bits;
            *output = (unsigned char)working_bits;
            output++;
            break;
        } else {
            /* Read more encoded bits */
            bits_to_read = undecoded_bits - working_bit_count;
            if (bits_to_read > 5)
                bits_to_read = 5;

            decoded_value = encoded_character_lookup[*input];
            if (decoded_value == -1)
                return -1;

            working_bits = (working_bits << bits_to_read) |
                           (decoded_value >> (5 - bits_to_read));
            working_bit_count += bits_to_read;

            input++;
        }
    }

    return output - decoded;
}

int zbase32_encode(unsigned char *encoded, const unsigned char *input,
                   unsigned int bits) {
    unsigned char *output = encoded;
    unsigned int character_index, excess_bits,
             unencoded_bits = bits, working_bits = 0, working_bit_count = 0;

    while (unencoded_bits > 0) {
        if (working_bit_count >= 5) {
            /* There are enough bits in working_bits to encode a character */
            working_bit_count -= 5;
            unencoded_bits -= 5;

            character_index = (working_bits >> working_bit_count) & 31;
            *output = alphabet[character_index];
            output++;
        } else if (unencoded_bits == working_bit_count) {
            /* There are no more bits beyond those in working_bits */
            character_index = (working_bits << (5 - unencoded_bits)) & 31;
            *output = alphabet[character_index];
            output++;
            break;
        } else if ((unencoded_bits - working_bit_count) >= 8) {
            /* Add a byte of input to working_bits */
            working_bits = (working_bits << 8) | (*input & 255);
            working_bit_count += 8;
            input++;
        } else {
            /* Add trailing input bits to working_bits */
            excess_bits = bits % 8;
            working_bits =
                (working_bits << excess_bits) |
                ((*input >> (8 - excess_bits)) & ((1 << excess_bits) - 1));
            working_bit_count += excess_bits;
        }
    }

    return output - encoded;
}