/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "verse_heap_type.h"

#include "pas_stream.h"

#if PAS_ENABLE_VERSE

void verse_heap_type_dump(const pas_heap_type* type, pas_stream* stream)
{
    pas_stream_printf(stream, "Size/Alignment = %zu", verse_heap_type_get_size(type));
}

#endif /* PAS_ENABLE_VERSE */

#endif /* LIBPAS_ENABLED */

