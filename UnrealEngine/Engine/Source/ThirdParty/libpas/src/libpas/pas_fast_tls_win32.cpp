// Copyright Epic Games, Inc. All Rights Reserved.

#include "pas_config.h"

#if LIBPAS_ENABLED

#if PAS_OS(WINDOWS)

#include "pas_fast_tls.h"
#include "pas_heap_lock.h"

struct LibPasFastTlsDestructor {
    ~LibPasFastTlsDestructor()
    {
        if (value)
            pas_fast_tls_destructor(value);
    }

    void* value { nullptr };
};

PAS_BEGIN_EXTERN_C;

__declspec(thread) void* pas_fast_tls_variable;
static thread_local LibPasFastTlsDestructor secondary_variable;

void pas_fast_tls_initialize_if_necessary(void) { }

void pas_fast_tls_set(void* value)
{
    pas_fast_tls_variable = value;
    if (value != PAS_FAST_TLS_DESTROYED)
        secondary_variable.value = value;
}

PAS_END_EXTERN_C;

#endif /* PAS_OS(WINDOWS) */

#endif /* LIBPAS_ENABLED */

