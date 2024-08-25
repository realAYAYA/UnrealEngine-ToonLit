/* Copyright Epic Games, Inc. All Rights Reserved. */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_fast_tls.h"
#include "pas_heap_lock.h"

#if defined(PAS_HAVE_PTHREAD_MACHDEP_H) && PAS_HAVE_PTHREAD_MACHDEP_H
bool pas_fast_tls_is_initialized = false;

void pas_fast_tls_initialize_if_necessary(void)
{
    pas_heap_lock_assert_held();
    if (pas_fast_tls_is_initialized)
        return;
    pthread_key_init_np(PAS_THREAD_LOCAL_KEY, pas_fast_tls_destructor);
    pas_fast_tls_is_initialized = true;
}
#elif !defined(_WIN32)
bool pas_fast_tls_is_initialized = false;
pthread_key_t pas_fast_tls_key;
__thread void* pas_fast_tls_variable;

void pas_fast_tls_initialize_if_necessary(void)
{
    pas_heap_lock_assert_held();
    if (pas_fast_tls_is_initialized)
        return;
    pthread_key_create(&pas_fast_tls_key, pas_fast_tls_destructor);
    pas_fast_tls_is_initialized = true;
}
#endif /* !defined(_WIN32) */

#endif /* LIBPAS_ENABLED */

