/* Copyright Epic Games, Inc. All Rights Reserved. */

#ifndef VERSE_HEAP_CONFIG_UE_H
#define VERSE_HEAP_CONFIG_UE_H

/* This has to be the max of what the system supports (so it's wrong for Darwin/arm64, for example).

   FIXME: This should just match PAS_GRANULE_DEFAULT_SHIFT, but I'm not sure that thing is dialed in. */
#define VERSE_HEAP_PAGE_SIZE_SHIFT 12u
#define VERSE_HEAP_PAGE_SIZE (1u << VERSE_HEAP_PAGE_SIZE_SHIFT)

/* The first page of a chunk is always the mark bits for the whole chunk.
 
   FIXME: on systems with 16KB pages, this results in a chunk that has more 16KB pages than what the
   chunk map can track. Is it better to optimize for chunk map layout, which implies making this be
   locked to small page size * 32 (which results in 12288 wasted bytes per chunk), or is it better to
   optimize for chunk layout (which means that each chunk map entry needs to be 128 bits on those
   systems)?

   Considering how many fewer chunks we'll have in that universe, it seems like it's better to
   optimize for chunk layout. Chunk map entries will become 4x larger, sure, but there will 4x fewer
   of them. Except - in that case, we need to be able to issue an atomic load from the chunk map in
   a very small number of cycles. That may not be possible on all of those systems, unfortunately.

   We could split the difference on those systems and allocate a 2x larger chunk (i.e. a 1MB chunk)
   and then use a 64-bit chunk map entry. Then, the mark bits page would be a bit of a waste - it
   would waste 8192 bytes.

   Or, we could have a chunk map that works even without atomic 128-bit loads of the entry. This
   may work out fine, simply because one a chunk map entry's state goes from empty to one of the
   other ones, it can only transition between empty and exactly that state. For example, you can
   go empty->medium->empty->medium, but you cannot go empty->medium->empty->small. This is ensured
   by the fact that small and medium pages never give up their VA in libpas. Additionally, the
   large heap uses its own cache (as part of the protocol for interoperating with the large sharing
   pool). Additionally, there is no small->empty transition, because we achieve that by just clearing
   all of the bits in the entry but still keeping it as a small entry.

   Therefore, we could just load the 128 bits nonatomically, and if we observe that the lowest bit is
   1 (i.e. small chunk) then we can safely rely on the rest of the bits. That's because in that case,
   those bits could have at worst torn from the empty state - but then they'd be all zero, which would
   imply to us that the page is empty. It's not wrong to assume that the page is empty, since that
   linearizes the chunk map lookup to the moment before the chunk map entry transitioned from empty. */
#define VERSE_HEAP_CHUNK_SIZE_SHIFT (VERSE_HEAP_PAGE_SIZE_SHIFT + VERSE_HEAP_MIN_ALIGN_SHIFT + 3u)
#define VERSE_HEAP_CHUNK_SIZE (1u << VERSE_HEAP_CHUNK_SIZE_SHIFT)

#define VERSE_HEAP_MIN_ALIGN_SHIFT 4u
#define VERSE_HEAP_MIN_ALIGN (1u << VERSE_HEAP_MIN_ALIGN_SHIFT)

#define VERSE_HEAP_SMALL_SEGREGATED_MIN_ALIGN_SHIFT VERSE_HEAP_MIN_ALIGN_SHIFT
#define VERSE_HEAP_SMALL_SEGREGATED_MIN_ALIGN (1u << VERSE_HEAP_SMALL_SEGREGATED_MIN_ALIGN_SHIFT)
#define VERSE_HEAP_SMALL_SEGREGATED_PAGE_SIZE 16384

#define VERSE_HEAP_MEDIUM_SEGREGATED_MIN_ALIGN_SHIFT 9u
#define VERSE_HEAP_MEDIUM_SEGREGATED_MIN_ALIGN (1u << VERSE_HEAP_MEDIUM_SEGREGATED_MIN_ALIGN_SHIFT)
#define VERSE_HEAP_MEDIUM_SEGREGATED_PAGE_SIZE VERSE_HEAP_CHUNK_SIZE

#endif /* VERSE_HEAP_CONFIG_UE_H */
