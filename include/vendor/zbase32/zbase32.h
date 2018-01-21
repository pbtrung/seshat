/*
 * Decode bits of encoded using z-base-32 and write the result into
 * decoded. If 8 is not a factor of bits, pad the output with zero bits
 * until a full byte is written.
 *
 * zbase32_decode(decoded, encoded, strlen(encoded) * 8);
 * strlen(decoded) = strlen(encoded);
 *
 * Returns the number of bytes written, or -1 if a byte that is not the
 * ASCII representation of a valid z-base-32 character is read.
 */
int zbase32_decode(unsigned char *decoded, const unsigned char *encoded,
                   unsigned int bits);

/*
 * Encode bits of input into z-base-32, and write the result into encoded.
 *
 * Returns the number of bytes written.
 */
int zbase32_encode(unsigned char *encoded, const unsigned char *input,
                   unsigned int bits);
