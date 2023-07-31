#pragma once
#include <stdint.h>
#include <emmintrin.h>

/* The list of the 4 usable parallel MD5 slots. */
typedef enum {
    PMD5_SLOT0 = 0,
    PMD5_SLOT1 = 1,
    PMD5_SLOT2 = 2,
    PMD5_SLOT3 = 3,
} pmd5_slot;

/* The parallel MD5 context structure. */
typedef struct {
    __m128i  state[4];
    uint64_t len[4];
} pmd5_context;

/* The status returned by the various functions below. */
typedef enum {
    PMD5_SUCCESS,
    PMD5_INVALID_SLOT,
    PMD5_UNALIGNED_UPDATE,
} pmd5_status;

/* Initializes all the 4 slots in the given pmd5 context. */
pmd5_status pmd5_init_all(pmd5_context * ctx);

/* Initializes a single slot out of the 4 in the given pmd5 context. */
pmd5_status pmd5_init_slot(pmd5_context * ctx, pmd5_slot slot);

/* Makes an MD5 update on all 4 slots in parallel, given the same exact length on all 4 streams.
   The stream pointers will be incremented accordingly.
   It is valid for a stream pointer to be NULL. Garbage will then be hashed into its corresponding slot.
   The argument length NEEDS to be a multiple of 64. If not, an error is returned, and the context is corrupted. */
pmd5_status pmd5_update_all_simple(pmd5_context * ctx, const uint8_t * data[4], uint64_t length);

/* Makes an MD5 update on all 4 slots in parallel, given 4 different lengths.
   The stream pointers will be incremented accordingly.
   The lengths will be decreased accordingly. Not all data might be consumed.
   It is valid for a stream pointer to be NULL. Garbage will then be hashed into its corresponding slot.
   The argument lengths NEEDS to contain 4 multiples of 64. If not, an error is returned, and the context is corrupted. */
pmd5_status pmd5_update_all(pmd5_context * ctx, const uint8_t * data[4], uint64_t lengths[4]);

/* Finishes all 4 slots at once. Fills in all 4 digests. */
pmd5_status pmd5_finish_all(pmd5_context * ctx, uint8_t digests[4][16]);

/* Finishes one slot. The other slots will be unnaffected. The finished slot can then continue to hash garbage using
   a NULL pointer as its stream argument, or needs to be reinitialized using pmd5_init_slot before being usable again. */
pmd5_status pmd5_finish_slot(pmd5_context * ctx, uint8_t digest[16], pmd5_slot slot);

/* Finishes one slot. Extra data is allowed to be passed on as an argument. Length DOESN'T need to be a
   multiple of 64. The other slots will be unnaffected. The finished slot can then continue to hash garbage using
   a NULL pointer as its stream argument, or needs to be reinitialized using pmd5_init_slot before being usable again. */
pmd5_status pmd5_finish_slot_with_extra(pmd5_context * ctx, uint8_t digest[16], pmd5_slot slot, const uint8_t * data, uint64_t length);

/* The normal MD5 context structure. */
typedef struct {
    uint32_t state[4];
    uint8_t  buffer[64];
    uint64_t len;
} md5_context;

/* Initializes an MD5 context. */
void md5_init(md5_context * ctx);

/* Makes an MD5 update on an MD5 context. */
void md5_update(md5_context * ctx, const uint8_t * data, uint64_t length);

/* Finishes an MD5 context, and fills the digest */
void md5_finish(md5_context * ctx, uint8_t digest[16]);

/* Insert a normal MD5 context into a given slot of a given parallel MD5 context. */
pmd5_status md5_to_pmd5(const md5_context * ctx, pmd5_context * pctx, pmd5_slot slot);

/* Extract a normal MD5 context from a given slot of a given parallel MD5 context. */
pmd5_status pmd5_to_md5(const pmd5_context * pctx, md5_context * ctx, pmd5_slot slot);
