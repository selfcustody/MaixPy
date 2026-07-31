
#include "rng.h"


// WARNING: this is NOT a hardware RNG and NOT cryptographically secure.
// The K210 has no TRNG, so rng_get() is a fixed-seed Yasmarang PRNG: its state
// is identical on every boot of every device, making the output fully
// deterministic and predictable. It exists only to satisfy callers such as lwIP
// and mbedtls that require an rng_get() symbol.
// Never use it for key material, nonces, or any other security purpose.

// Yasmarang random number generator by Ilya Levin
// http://www.literatecode.com/yasmarang
static uint32_t pyb_rng_yasmarang(void) {
    static uint32_t pad = 0xeda4baba, n = 69, d = 233;
    static uint8_t dat = 0;

    pad += dat + d * n;
    pad = (pad << 3) + (pad >> 29);
    n = pad | 2;
    d ^= (pad << 31) + (pad >> 1);
    dat ^= (char)pad ^ (d >> 8) ^ 1;

    return pad ^ (d << 5) ^ (pad >> 18) ^ (dat << 1);
}

uint32_t rng_get(void) {
    return pyb_rng_yasmarang();
}

