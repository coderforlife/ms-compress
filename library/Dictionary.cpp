#include "Dictionary.h"

// Implementation designed for being extremely fast at the expense of memory
// usage. The base memory usage is 512 KB (or 768 KB on 64-bit systems). More
// memory is always allocated but only as much as needed. Larger sized chunks
// will consume more memory. For a series of 4 KB chunks, the extra consumed
// memory is around 20-80 KB. For a series of 64 KB chunks, it is 200-800 KB.

// This implementation is ~30x faster than the 576 KB fixed-size Dictionary!

#ifdef __cplusplus_cli
#pragma unmanaged
#endif

#if defined(_MSC_VER) && defined(NDEBUG)
#pragma optimize("t", on)
#endif

#define MAX_BYTE	0x100					// maximum byte value (+1 for 0)
#define MIN(a, b) (((a) < (b)) ? (a) : (b))	// minimum of 2 values

// An entry within the dictionary, using a dynamically resized array of positions
typedef struct _Entry // 8+ bytes (12+ bytes on 64-bit systems)
{
	const_bytes* pos;
	uint16_t size, cap;
} Entry;

// The dictionary
struct _Dictionary // 512+ KB (768+ KB on 64-bit systems)
{
	Entry entries[MAX_BYTE][MAX_BYTE];
};

// Creates and returns an uninitialized dictionary struct
Dictionary* Dictionary_Create()
{
	Dictionary* d = (Dictionary*)malloc(sizeof(Dictionary));
	if (d) memset(d, 0, sizeof(Dictionary)); // need to set pos and cap to 0, might as well set size to 0
#ifdef PRINT_ERRORS
	else PRINT_ERROR("Dictionary Creation Error: malloc failed\n");
#endif
	return d;
}

// Destroys a dictionary struct
void Dictionary_Destroy(Dictionary* d)
{
	uint_fast16_t i, j;
	for (i = 0; i < MAX_BYTE; ++i)
		for (j = 0; j < MAX_BYTE; ++j)
			free(d->entries[i][j].pos);
	free(d);
}

// Resets a dictionary struct ready to start a new chunk
// This should also be called after Dictionary_Create and before any Dictionary_Add/Dictionary_Find
// Returns true on success, false on error (and sets errno)
bool Dictionary_Reset(Dictionary* d)
{
	uint_fast16_t i, j;
	for (i = 0; i < MAX_BYTE; ++i)
		for (j = 0; j < MAX_BYTE; ++j)
			d->entries[i][j].size = 0;
	return true;
}

// Adds data to the dictionary, which will be used as a starting point during future finds
// Max length is how many bytes can be read from data, regardless of the end of the chunk
// Returns true on success, false on error
bool Dictionary_Add(Dictionary* d, const_bytes data, const size_t max_len)
{
	if (max_len >= 2)
	{
		const byte x = data[0], y = data[1];
		Entry* e = d->entries[x]+y;
		if (e->size >= e->cap)
		{
			const_bytes *temp = (const_bytes*)realloc(e->pos, (e->cap=(e->cap?((e->cap==0x8000)?0xFFFF:(e->cap<<1)):8))*sizeof(const_bytes));
			if (temp == NULL)
			{
				PRINT_ERROR("Dictionary Add Error: realloc failed\n");
				Dictionary_Destroy(d);
				return false;
			}
			e->pos = temp;
		}
		e->pos[e->size++] = data;
	}
	return true;
}

// Finds the best symbol in the dictionary for the data at u[pos]
// Returns the length of the string found, or 0 if nothing of length >= 3 was found
// offset is set to the offset from the current position to the string
uint_fast16_t Dictionary_Find(const Dictionary* d, const Dictionary* d2, const_bytes data, const uint_fast16_t max_len, const_bytes search, uint_fast16_t* offset)
{
	static const Entry DummyEntry = { NULL, 0, 0 }; // using this instead of NULL reduces a lot of checks

	if (max_len >= 3 && data-search > 0)
	{
		const byte x = data[0], y = data[1];
		const Entry* e = d->entries[x]+y, *e2 = d2 ? d2->entries[x]+y : &DummyEntry;
		if (e->size || e2->size) // a match is possible
		{
			const byte z = data[2];
			uint_fast16_t l = 0, o;
			int_fast32_t ep = e->size - 1; // need to support all uint16 values and <0

			// Try short repeats - this does not use the Dictionary at all
			if (x == z && y == data[-1])
			{
				if (x == y) // x == y == z == data[-1]
				{
					// Repeating the last byte
					o = 1;
					l = 3;
					while (l < max_len && data[l] == x)	{ ++l; }
					--ep;
					if (data-search > 1 && x == data[-2])
						--ep;
				}
				else if (data-search > 1 && x == data[-2]) // x == z == data[-2], y == data[-1]
				{
					// Repeating the last two bytes
					o = 2;
					l = 3;
					while (l < max_len && data[l] == y)	{ ++l; if (l < max_len && data[l] == x) { ++l; } else break; }
					--ep;
				}

				// Found the best match, stop now
				if (l == max_len) { *offset = o; return l; }
			}

			// Do an exhaustive search (with the possible positions)
			if (ep < 0) { ep += (e=e2)->size-1; e2 = &DummyEntry; }
			do
			{
				for (; ep >= 0 && e->pos[ep] >= search; --ep)
				{
					const const_bytes ss = e->pos[ep];
					if (ss[2] == z)
					{
						const_bytes s = ss+3;
						uint_fast16_t i = 3;
						if (s == data) { s = ss; }
						while (i < max_len && data[i] == *s)
						{
							++i;
							if (++s == data) { s = ss; } // allow looping back, can have l > o
						}
						if (i > l) { o = (uint_fast16_t)(data-ss); if ((l = i) == max_len) { break; } }
					}
				}
				ep = (e=e2)->size - 1;
				e2 = &DummyEntry;
			} while (ep >= 0);

			// Found a match, return it
			if (l >= 3)
			{
				*offset = o;
				return l;
			}
		}
	}

	// No match found, return 0
	return 0;
}
