#pragma once
#include <cstring>
#define crypto_hash_sha256_BYTES 32

// Fixed protocol marker for host tests; not a SHA-256 implementation.
inline int crypto_hash_sha256(unsigned char* out, const unsigned char*, unsigned long long)
{
    memset(out, 0xa5, crypto_hash_sha256_BYTES);
    return 0;
}
