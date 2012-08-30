#include "compression-api.h"

// Tabulation Hashing
// Will hash 3 bytes at a time to a number from 0 to MAX_HASH-1 (inclusive)

//            SEED   A            C           M            S
//MSVCRT                  214013     2531011  (1 << 32)    16
//RtlUniform         0x 7FFFFFED  0x7FFFFFC3  (1 << 31)-1  15
//Java          *    0x5DEECE66D          11  (1 << 48)    48-15       * processed with (SEED ^ 0x5DEECE66Dul) % (1ull << 48)
//GLIBC              0x 41C64E6D       12345  (1 << 32),   16
//CarbonLib                16807           0  (1 << 31)-1, 15          Also called MINSTD
// Different hash tables will not effect compression ratio but may effect speed.

// The real Microsoft one for XPRESS is similar to a LCG but not quite the same. It uses a seed of 0x13579BDFul. The inside loop of the LCG is changed to:
//			uint32_t a = 0, b = hash, c = 0x87654321u;
//			int k;
//			for (k = 0; k < 32; ++k)
//			{
//				uint32_t d;
//				a -= 0x61C88647u; b += a; c += a;
//				d = ((c + 0x3B2A1908) ^ (b + 0x43B2A19) ^ (a - 0x789ABCDF)) + hash;
//				hash = ((a + d) ^ (c + (d >> 5)) ^ (b + (d << 3))) + d;
//			}
//			table[j] = (hash -= 0x789ABCDFu) & (MAX_HASH - 1);

template<uint32_t SEED, uint32_t A, uint32_t C, uint32_t M, uint32_t S, uint32_t MaxHash>
class LCG
{
private:
	uint16_t hashes[3][0x100];

	// Initializes the tabulation data using a linear congruential generator
	// The values are essentially random and evenly distributed
	LCG()
	{
		uint32_t hash = SEED;
		for (int i = 0; i < 3; ++i)
		{
			uint16_t* table = hashes[i];
			for (int j = 0; j < 0x100; ++j)
			{
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable:4724) // warning C4724: potential mod by 0
#pragma warning(disable:4127) // warning C4127: conditional expression is constant
#endif
				if (M)	hash = (hash * A + C) % M; // if M == 0, then M is actually max int, no need to do any division
				else	hash = (hash * A + C);
				table[j] = (hash >> S) & (MaxHash - 1);
#ifdef _MSC_VER
#pragma warning(pop)
#endif
			}
		}
	}
	static LCG<SEED, A, C, M, S, MaxHash> lcg;

public:
	inline static uint_fast16_t Hash(const const_bytes x) { return lcg.hashes[0][x[0]] ^ lcg.hashes[1][x[1]] ^ lcg.hashes[2][x[2]]; }
};

template<uint32_t SEED, uint32_t A, uint32_t C, uint32_t M, uint32_t S, uint32_t MaxHash>
LCG<SEED, A, C, M, S, MaxHash> LCG<SEED, A, C, M, S, MaxHash>::lcg;
