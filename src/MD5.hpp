/* MD5
 *  converted to C++ class by Frank Thilo (thilo@unix-ag.org)
 *  for bzflag (http://www.bzflag.org)
 *
 *  based on:
 *
 *  md5.h and md5.c
 *  reference implementation of RFC 1321
 *
 *  Copyright (C) 1991-2, RSA Data Security, Inc. Created 1991. All
 *  rights reserved.
 *
 *  License to copy and use this software is granted provided that it
 *  is identified as the "RSA Data Security, Inc. MD5 Message-Digest
 *  Algorithm" in all material mentioning or referencing this software
 *  or this function.
 *
 *  License is also granted to make and use derivative works provided
 *  that such works are identified as "derived from the RSA Data
 *  Security, Inc. MD5 Message-Digest Algorithm" in all material
 *  mentioning or referencing the derived work.
 *
 *  RSA Data Security, Inc. makes no representations concerning either
 *  the merchantability of this software or the suitability of this
 *  software for any particular purpose. It is provided "as is"
 *  without express or implied warranty of any kind.
 *
 *  These notices must be retained in any copies of any part of this
 *  documentation and/or software.
 *
 */

#ifndef MD5_HPP
#define MD5_HPP

#include <stdint.h>

#include <string>

// a small class for calculating MD5 hashes of strings or byte arrays
// it is not meant to be fast or secure
//
// usage: 1) feed it blocks of uchars with update()
//      2) finalize()
//      3) get hexdigest() string
//      or
//      MD5(std::string).hexdigest()

static_assert(sizeof(char) == sizeof(uint8_t), "char isn't 8 bits");
static_assert(sizeof(int) == sizeof(uint32_t), "int isn't 32 bits");
using uint128_t = __uint128_t;

class[[gnu::packed]] MD5 {
public:
    MD5();
    explicit MD5(const std::string& text);
    void update(const unsigned char* buf, uint32_t length);
    void update(const char* buf, uint32_t length);
    void finalize();
    uint128_t getDigest();

private:
    constexpr static int kBlockSize = 64;

    void transform(const uint8_t block[kBlockSize]);
    static void decode(uint32_t output[], const uint8_t input[], uint32_t len);
    static void encode(uint8_t output[], const uint32_t input[], uint32_t len);

    uint8_t
        m_buffer[kBlockSize];  // bytes that didn't fit in last 64 byte chunk
    uint64_t m_count;          // counter for number of bits
    uint32_t m_state[4];       // digest so far
    uint128_t m_digest;        // the result
    bool m_finalized;

    // low level logic operations
    static uint32_t F(uint32_t x, uint32_t y, uint32_t z);
    static uint32_t G(uint32_t x, uint32_t y, uint32_t z);
    static uint32_t H(uint32_t x, uint32_t y, uint32_t z);
    static uint32_t I(uint32_t x, uint32_t y, uint32_t z);
    static uint32_t rotate_left(uint32_t x, int n);
    static void FF(uint32_t & a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
                   uint32_t s, uint32_t ac);
    static void GG(uint32_t & a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
                   uint32_t s, uint32_t ac);
    static void HH(uint32_t & a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
                   uint32_t s, uint32_t ac);
    static void II(uint32_t & a, uint32_t b, uint32_t c, uint32_t d, uint32_t x,
                   uint32_t s, uint32_t ac);
};

#endif  // MD5_HPP
