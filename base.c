#include <stdio.h>
#include <stdint.h>


typedef struct {
    int32_t index;
    int64_t padding;    // seems to serve no purpose.
    uint64_t state[624];
    uint32_t const_1388;    // always 0xFFFFFFFFFFFFFFFF, probably part of reference implementation?
    uint32_t const_138C;
} RNG_State;

typedef struct {
    RNG_State* mt_state;
    uint32_t bits_in_rand;
    uint64_t max_rand_val;       // seems useless, set to UINT64_MAX
} RNG;

uint64_t sdbm2(char *str)   // core of Math.seedRandomString
{
    uint64_t hash = 0;
    int c;

    while ((c = *str++))
        hash = c + (hash << 6) + (hash << 16) - hash;

    return hash;
}

void mt_init(RNG_State *state, uint64_t seed)   // internal
{
    const uint64_t magic_mult = 6364136223846793005ULL;
    uint64_t curr = seed;
    state->state[0] = seed;
    for (int i = 1; i <= 312; i++) {
        curr = ((curr ^ (curr >> 62))) * magic_mult + i;
        state->state[i] = curr;
    }
}

static inline uint64_t twist(uint64_t u, uint64_t v) {      // internal
    uint64_t z = (u & 0xFFFFFFFF80000000ULL) | (v & 0x000000007FFFFFFFULL);

    uint64_t magic = 0;
    if (z & 1) {
        magic = 0xB5026F5AA96619E9ULL;
    }

    return (z >> 1) ^ magic;
}

static inline uint64_t get_state(RNG_State* s, int index) {     // internal
    if (index < 624) {
        return s->state[index];
    } else {
        return 0x5555555555555555;
    }
}

static inline void refill_half(RNG_State* s) {      // internal
    for (int i = 1; i <= 312; ++i) {
        uint64_t twisted = twist(s->state[i - 1], s->state[i]);
        s->state[i + 311] = s->state[i + 155] ^ twisted;
    }
}

static inline void refill_full_and_reset(RNG_State* s) {    // internal
    for (int k = 1; k <= 156; ++k) {
        uint64_t twisted = twist(s->state[k + 312], get_state(s, k + 313));
        s->state[k] = get_state(s, k + 468) ^ twisted;
    }

    for (int k = 157; k <= 311; ++k) {
        uint64_t twisted = twist(s->state[k + 312], get_state(s, k + 313));
        s->state[k] = s->state[k - 156] ^ twisted;
    }

    s->state[311] = s->state[155] ^ twist(s->state[623], s->state[0]);

    s->index = 0;
}

uint64_t MT19937_variant(RNG_State* s) {    // internal
    if (s->index == 312) {
        refill_half(s);
    } else if (s->index >= 624) {
        refill_full_and_reset(s);
    }

    uint64_t x = s->state[s->index];
    s->index++;

    x ^= (x >> 29) & 0x5555555555555555ULL;
    x ^= (x << 17) & 0x71D67FFFEDA60000ULL;
    x ^= (x << 37) & 0xFFF7EEE000000000ULL;
    x ^= (x >> 43);
    
    return x;
}


uint32_t rand(RNG* rng, uint32_t low, uint32_t high) {  // effectively Math.rand
    uint32_t range = high - low + 1;
    if (range <= 1)
    {
        return low;
    }

    while (1) {
        uint64_t v24 = 0;
        uint64_t v1C = 0;

        while (v1C < (range - 1)) {
            v24 <<= rng->bits_in_rand;
            v1C <<= rng->bits_in_rand;

            uint64_t rand_val;
            do {
                rand_val = MT19937_variant(rng->mt_state);
            } while (rand_val > rng->max_rand_val);

            v24 |= rand_val;
            v1C |= rng->max_rand_val;
        }

        uint64_t q_v1C = v1C / range;
        uint64_t q_v24 = v24 / range;

        if (q_v24 < q_v1C) {
            return (v24 % range) + low;
        }

        uint32_t rem_v1C = v1C % range;
        
        if (rem_v1C == (range - 1) && ((v1C >> 32) == 0)) {
            return (v24 % range) + low;
        }
    }
}

int main(void)
{
    RNG_State state = {312, 0, {0}, 0x55555555, 0x55555555};
    mt_init(&state, sdbm2("pasword"));
    RNG rng = {&state, 64, UINT64_MAX};
    uint64_t accum = 0;

    for (int i = 0; i < 9000; i++)
    {
        uint32_t val = rand(&rng, 1, 100);
        printf("%u\n", val);
    }

    return 0;
}

//
//  this will give output equivalent to the script
//  Math.seedRandomString("pasword");
//  for(local itr=0;itr<9000;itr+=1)
//  {
//      logInfo(Math.rand(1, 100));
//  }
//
