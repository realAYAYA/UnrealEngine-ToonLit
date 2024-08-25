/*
 * Copyright (c) 2019-2022 Apple Inc. All rights reserved.
 * Copyright Epic Games, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_scavenger.h"

#include <math.h>
#include "pas_all_heaps.h"
#include "pas_baseline_allocator_table.h"
#include "pas_compact_expendable_memory.h"
#include "pas_deferred_decommit_log.h"
#include "pas_dyld_state.h"
#include "pas_epoch.h"
#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_large_expendable_memory.h"
#include "pas_lock.h"
#include "pas_page_sharing_pool.h"
#include "pas_status_reporter.h"
#include "pas_thread_local_cache.h"
#include "pas_utility_heap.h"
#include "verse_heap_mark_bits_page_commit_controller.h"
#include <stdio.h>
#ifndef _WIN32
#include <unistd.h>
#endif

static const bool verbose = false;
static bool is_shut_down_enabled = true;

bool pas_scavenger_is_enabled = true;
bool pas_scavenger_eligibility_notification_has_been_deferred = false;
pas_scavenger_state pas_scavenger_current_state = pas_scavenger_state_no_thread;
unsigned pas_scavenger_should_suspend_count = 0;
pas_scavenger_data* pas_scavenger_data_instance = NULL;

double pas_scavenger_deep_sleep_timeout_in_milliseconds = 10. * 1000.;
double pas_scavenger_period_in_milliseconds = 125.;
uint64_t pas_scavenger_max_epoch_delta = 300ll * 1000ll * 1000ll;

#if PAS_OS(DARWIN)
static _Atomic qos_class_t pas_scavenger_requested_qos_class = QOS_CLASS_USER_INITIATED;

void pas_scavenger_set_requested_qos_class(qos_class_t qos_class)
{
    pas_scavenger_requested_qos_class = qos_class;
}
#endif

pas_scavenger_activity_callback pas_scavenger_did_start_callback = NULL;
pas_scavenger_activity_callback pas_scavenger_completion_callback = NULL;
pas_scavenger_activity_callback pas_scavenger_will_shut_down_callback = NULL;

static pas_scavenger_data* ensure_data_instance(pas_lock_hold_mode heap_lock_hold_mode)
{
    pas_scavenger_data* instance;
    
    instance = pas_scavenger_data_instance;

    pas_compiler_fence();

    if (instance)
        return instance;
    
    pas_heap_lock_lock_conditionally(heap_lock_hold_mode);
    instance = pas_scavenger_data_instance;
    if (!instance) {
        instance = pas_immortal_heap_allocate(
            sizeof(pas_scavenger_data),
            "pas_scavenger_data",
            pas_object_allocation);

        pas_system_mutex_construct(&instance->lock);
        pas_system_condition_construct(&instance->cond);

        pas_fence();
        
        pas_scavenger_data_instance = instance;
    }
    pas_heap_lock_unlock_conditionally(heap_lock_hold_mode);
    
    return instance;
}

static bool handle_expendable_memory(pas_expendable_memory_scavenge_kind kind)
{
    bool should_go_again = false;
    pas_heap_lock_lock();
    should_go_again |= pas_compact_expendable_memory_scavenge(kind);
    should_go_again |= pas_large_expendable_memory_scavenge(kind);
    pas_heap_lock_unlock();
    return should_go_again;
}

static pas_thread_return_type scavenger_thread_main(void* arg)
{
    pas_scavenger_data* data;
    pas_scavenger_activity_callback did_start_callback;
#if PAS_OS(DARWIN)
    qos_class_t configured_qos_class;
#endif
    
    PAS_UNUSED_PARAM(arg);
    
    PAS_ASSERT(pas_scavenger_current_state == pas_scavenger_state_polling);

    if (verbose) {
        pas_log("Scavenger is running in thread " PAS_SYSTEM_THREAD_ID_FORMAT "\n",
                pas_get_current_system_thread_id());
    }

#if PAS_OS(DARWIN) || PAS_PLATFORM(PLAYSTATION)
#if defined(PAS_BMALLOC) && PAS_BMALLOC
    pthread_setname_np("JavaScriptCore libpas scavenger");
#else
    pthread_setname_np("libpas scavenger");
#endif
#endif

    did_start_callback = pas_scavenger_did_start_callback;
    if (did_start_callback)
        did_start_callback();
    
    data = ensure_data_instance(pas_lock_is_not_held);
    
#if PAS_OS(DARWIN)
    configured_qos_class = pas_scavenger_requested_qos_class;
    pthread_set_qos_class_self_np(configured_qos_class, 0);
#endif

    for (;;) {
        pas_page_sharing_pool_scavenge_result scavenge_result;
        bool should_shut_down;
        double time_in_milliseconds;
        double absolute_timeout_in_milliseconds_for_period_sleep;
        pas_scavenger_activity_callback completion_callback;
        pas_thread_local_cache_decommit_action thread_local_cache_decommit_action;
        bool should_go_again;
        uint64_t epoch;
        uint64_t delta;
        uint64_t max_epoch;
        bool did_overflow;
#if PAS_OS(DARWIN)
        qos_class_t current_qos_class;
#endif

#if PAS_OS(DARWIN)
        current_qos_class = pas_scavenger_requested_qos_class;
        if (configured_qos_class != current_qos_class) {
            configured_qos_class = current_qos_class;
            pthread_set_qos_class_self_np(configured_qos_class, 0);
        }
#endif

        should_go_again = false;
        
        if (verbose)
            printf("Scavenger is running.\n");

#if PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY
        pas_local_allocator_refill_efficiency_lock_lock();
        pas_log("%d: Refill efficiency: %lf\n",
                getpid(),
                pas_local_allocator_refill_efficiency_sum / pas_local_allocator_refill_efficiency_n);
        pas_local_allocator_refill_efficiency_lock_unlock();
#endif /* PAS_LOCAL_ALLOCATOR_MEASURE_REFILL_EFFICIENCY */
        
        should_go_again |=
            pas_baseline_allocator_table_for_all(pas_allocator_scavenge_request_stop_action);

        should_go_again |=
            pas_utility_heap_for_all_allocators(pas_allocator_scavenge_request_stop_action,
                                                pas_lock_is_not_held);
        
		thread_local_cache_decommit_action = pas_thread_local_cache_decommit_if_possible_action;
        should_go_again |=
            pas_thread_local_cache_for_all(pas_allocator_scavenge_request_stop_action,
                                           pas_deallocator_scavenge_flush_log_if_clean_action,
                                           thread_local_cache_decommit_action);

        should_go_again |= handle_expendable_memory(pas_expendable_memory_scavenge_periodic);

        /* For the purposes of performance tuning, as well as some of the scavenger tests, the epoch
           is time in nanoseconds.
           
           But for some tests, including some scavenger tests, the epoch is just a counter.
           
           This code is engineered to kind of limp along when the epoch is a counter, but it doesn't
           actually achieve its full purpose unless the epoch really is time. */
        epoch = pas_get_epoch();
        delta = pas_scavenger_max_epoch_delta;

        did_overflow = pas_sub_uint64_overflow(epoch, delta, &max_epoch);
        if (did_overflow)
            max_epoch = PAS_EPOCH_MIN;

        if (verbose)
            pas_log("epoch = %llu, delta = %llu, max_epoch = %llu\n", (unsigned long long)epoch, (unsigned long long)delta, (unsigned long long)max_epoch);

        scavenge_result = pas_physical_page_sharing_pool_scavenge(max_epoch);

        switch (scavenge_result.take_result) {
        case pas_page_sharing_pool_take_none_available:
            break;
            
        case pas_page_sharing_pool_take_none_within_max_epoch:
            should_go_again = true;
            break;
            
        case pas_page_sharing_pool_take_success: {
            PAS_ASSERT(!"Should not see pas_page_sharing_pool_take_success.");
            break;
        }

        case pas_page_sharing_pool_take_locks_unavailable: {
            PAS_ASSERT(!"Should not see pas_page_sharing_pool_take_locks_unavailable.");
            break;
        } }

        if (verbose) {
            pas_log("%d: %.0lf: scavenger freed %zu bytes (%s, should_go_again = %s).\n",
                    pas_getpid(), pas_get_time_in_milliseconds_for_system_condition(), scavenge_result.total_bytes,
                    pas_page_sharing_pool_take_result_get_string(scavenge_result.take_result),
                    should_go_again ? "yes" : "no");
        }

        pas_heap_lock_lock();
        should_go_again |= pas_bootstrap_free_heap_scavenge_periodic();
        pas_heap_lock_unlock();

        should_go_again |= pas_immortal_heap_scavenge_periodic();
		should_go_again |= verse_heap_mark_bits_page_commit_controller_scavenge_periodic();
        
        completion_callback = pas_scavenger_completion_callback;
        if (completion_callback)
            completion_callback();
        
        should_shut_down = false;
        
        pas_system_mutex_lock(&data->lock);
        
        PAS_ASSERT(pas_scavenger_current_state == pas_scavenger_state_polling ||
                   pas_scavenger_current_state == pas_scavenger_state_deep_sleep);
        
        time_in_milliseconds = pas_get_time_in_milliseconds_for_system_condition();
        
        if (verbose)
            printf("Finished a round of scavenging at %.2lf.\n", time_in_milliseconds);
        
        /* By default we need to sleep for a short while and then try again. */
        absolute_timeout_in_milliseconds_for_period_sleep =
            time_in_milliseconds + pas_scavenger_period_in_milliseconds;

        if (should_go_again) {
            if (verbose)
                printf("Waiting for a period.\n");

            /* This field is accessed a lot by other threads, so don't write to it if we don't
               have to. */
            if (pas_scavenger_current_state != pas_scavenger_state_polling)
                pas_scavenger_current_state = pas_scavenger_state_polling;
        } else if (PAS_LIKELY(is_shut_down_enabled)) {
            double absolute_timeout_in_milliseconds_for_deep_pre_sleep;
            
            if (pas_scavenger_current_state == pas_scavenger_state_polling) {
                if (verbose)
                    printf("Will consider deep sleep.\n");
                
                /* do one more round of polling but this time indicating that it's the last
                   chance. */
                pas_scavenger_current_state = pas_scavenger_state_deep_sleep;
            } else {
                if (verbose)
                    printf("Considering deep sleep.\n");
                
                PAS_ASSERT(pas_scavenger_current_state == pas_scavenger_state_deep_sleep);
                
                absolute_timeout_in_milliseconds_for_deep_pre_sleep =
                    time_in_milliseconds + pas_scavenger_deep_sleep_timeout_in_milliseconds;
                
                /* need to deep sleep and then shut down. */
                while (pas_get_time_in_milliseconds_for_system_condition() < absolute_timeout_in_milliseconds_for_deep_pre_sleep
                       && !pas_scavenger_should_suspend_count
                       && pas_scavenger_current_state == pas_scavenger_state_deep_sleep) {
                    pas_system_condition_timed_wait(
                        &data->cond, &data->lock,
                        absolute_timeout_in_milliseconds_for_deep_pre_sleep);
                }
                
                if (pas_scavenger_current_state == pas_scavenger_state_deep_sleep)
                    should_shut_down = true;
            }
        }
        
        if (PAS_LIKELY(is_shut_down_enabled)) {
            while (pas_get_time_in_milliseconds_for_system_condition() < absolute_timeout_in_milliseconds_for_period_sleep
                   && !pas_scavenger_should_suspend_count) {
                pas_system_condition_timed_wait(
                    &data->cond, &data->lock,
                    absolute_timeout_in_milliseconds_for_period_sleep);
            }

            should_shut_down |= !!pas_scavenger_should_suspend_count;

            if (should_shut_down) {
                pas_scavenger_current_state = pas_scavenger_state_no_thread;
                pas_system_condition_broadcast(&data->cond);
            }
        }

        pas_system_mutex_unlock(&data->lock);
        
        if (should_shut_down) {
            pas_scavenger_activity_callback shut_down_callback;

            shut_down_callback = pas_scavenger_will_shut_down_callback;
            if (shut_down_callback)
                shut_down_callback();
            
            if (verbose)
                printf("Killing the scavenger.\n");
            return PAS_THREAD_RETURN_VALUE;
        }
    }

    PAS_ASSERT(!"Should not be reached");
    return PAS_THREAD_RETURN_VALUE;
}

bool pas_scavenger_did_create_eligible(void)
{
    if (pas_scavenger_current_state == pas_scavenger_state_polling)
        return false;
    
    if (!pas_scavenger_is_enabled)
        return false;
    
    if (pas_scavenger_eligibility_notification_has_been_deferred)
        return true;
    
    pas_fence();
    
    pas_scavenger_eligibility_notification_has_been_deferred = true;
    return true;
}

void pas_scavenger_notify_eligibility_if_needed(void)
{
    pas_scavenger_data* data;
    
    if (!pas_scavenger_is_enabled)
        return;
    
    if (!pas_scavenger_eligibility_notification_has_been_deferred)
        return;
    
    if (pas_scavenger_should_suspend_count)
        return;

    if (!pas_dyld_is_libsystem_initialized())
        return;
    
    pas_fence();
    
    pas_scavenger_eligibility_notification_has_been_deferred = false;
    
    pas_fence();
    
    if (pas_scavenger_current_state == pas_scavenger_state_polling)
        return;
    
    if (verbose)
        printf("It's not polling so need to do something.\n");
    
    data = ensure_data_instance(pas_lock_is_not_held);
    pas_system_mutex_lock(&data->lock);
    
    if (pas_scavenger_should_suspend_count)
        goto done;
    
    if (pas_scavenger_current_state == pas_scavenger_state_no_thread) {
        pas_scavenger_current_state = pas_scavenger_state_polling;
        pas_create_detached_thread(scavenger_thread_main, NULL);
    }
    
    if (pas_scavenger_current_state == pas_scavenger_state_deep_sleep) {
        pas_scavenger_current_state = pas_scavenger_state_polling;
        pas_system_condition_broadcast(&data->cond);
    }
    
done:
    pas_system_mutex_unlock(&data->lock);

    pas_status_reporter_start_if_necessary();
}

void pas_scavenger_suspend(void)
{
    pas_scavenger_data* data;
    data = ensure_data_instance(pas_lock_is_not_held);
    pas_system_mutex_lock(&data->lock);
    
    pas_scavenger_should_suspend_count++;
    PAS_ASSERT(pas_scavenger_should_suspend_count);
    
    while (pas_scavenger_current_state != pas_scavenger_state_no_thread)
        pas_system_condition_wait(&data->cond, &data->lock);
    
    pas_system_mutex_unlock(&data->lock);
}

void pas_scavenger_resume(void)
{
    pas_scavenger_data* data;
    data = ensure_data_instance(pas_lock_is_not_held);
    pas_system_mutex_lock(&data->lock);
    
    PAS_ASSERT(pas_scavenger_should_suspend_count);
    
    pas_scavenger_should_suspend_count--;
    
    pas_system_mutex_unlock(&data->lock);
    
    /* Just assume that there are empty pages to be scavenged. We wouldn't have been keeping
       track perfectly while the scavenger was suspended. For example, we would not have
       remembered if there still hadd been empty pages at the time that the scavenger had been
       shut down. */
    pas_scavenger_did_create_eligible();
    
    pas_scavenger_notify_eligibility_if_needed();
}

void pas_scavenger_clear_local_tlcs(void)
{
    pas_thread_local_cache* cache;
    
    cache = pas_thread_local_cache_try_get();
    if (cache)
        pas_thread_local_cache_shrink(cache, pas_lock_is_not_held);
}

void pas_scavenger_clear_all_non_tlc_caches(void)
{
    pas_baseline_allocator_table_for_all(pas_allocator_scavenge_force_stop_action);

    pas_utility_heap_for_all_allocators(pas_allocator_scavenge_force_stop_action,
                                        pas_lock_is_not_held);
}

void pas_scavenger_clear_all_caches_except_remote_tlcs(void)
{
	pas_scavenger_clear_local_tlcs();
    pas_scavenger_clear_all_non_tlc_caches();
}

void pas_scavenger_clear_all_caches(void)
{
    pas_scavenger_clear_all_caches_except_remote_tlcs();
    
    pas_thread_local_cache_for_all(pas_allocator_scavenge_force_stop_action,
                                   pas_deallocator_scavenge_flush_log_action,
                                   pas_thread_local_cache_decommit_if_possible_action);
}

void pas_scavenger_decommit_expendable_memory(void)
{
    handle_expendable_memory(pas_expendable_memory_scavenge_forced);
}

void pas_scavenger_fake_decommit_expendable_memory(void)
{
    handle_expendable_memory(pas_expendable_memory_scavenge_forced_fake);
}

size_t pas_scavenger_decommit_bootstrap_free_heap(void)
{
    size_t result;
    pas_heap_lock_lock();
    result = pas_bootstrap_free_heap_decommit();
    pas_heap_lock_unlock();
    return result;
}

size_t pas_scavenger_decommit_immortal_heap(void)
{
    return pas_immortal_heap_decommit();
}

bool pas_scavenger_decommit_verse_heap_mark_bits(void)
{
	return verse_heap_mark_bits_page_commit_controller_decommit_if_possible();
}

size_t pas_scavenger_decommit_free_memory(void)
{
    pas_page_sharing_pool_scavenge_result result;

    result = pas_physical_page_sharing_pool_scavenge(PAS_EPOCH_MAX);
    
    PAS_ASSERT(result.take_result == pas_page_sharing_pool_take_none_available);

    return result.total_bytes;
}

void pas_scavenger_decommit_everything(void)
{
    pas_scavenger_decommit_expendable_memory();
    pas_scavenger_decommit_free_memory();
    pas_scavenger_decommit_bootstrap_free_heap();
    pas_scavenger_decommit_immortal_heap();
	pas_scavenger_decommit_verse_heap_mark_bits();
}

void pas_scavenger_run_synchronously_now(void)
{
    pas_scavenger_clear_all_caches();
	pas_scavenger_decommit_everything();
}

void pas_scavenger_do_everything_except_remote_tlcs(void)
{
    pas_scavenger_clear_all_caches_except_remote_tlcs();
	pas_scavenger_decommit_everything();
}

void pas_scavenger_perform_synchronous_operation(
    pas_scavenger_synchronous_operation_kind kind)
{
    switch (kind) {
    case pas_scavenger_invalid_synchronous_operation_kind:
        PAS_ASSERT(!"Should not be reached");
        return;
	case pas_scavenger_clear_local_tlcs_kind:
		pas_scavenger_clear_local_tlcs();
		return;
    case pas_scavenger_clear_all_non_tlc_caches_kind:
        pas_scavenger_clear_all_non_tlc_caches();
        return;
    case pas_scavenger_clear_all_caches_except_remote_tlcs_kind:
        pas_scavenger_clear_all_caches_except_remote_tlcs();
        return;
    case pas_scavenger_clear_all_caches_kind:
        pas_scavenger_clear_all_caches();
        return;
    case pas_scavenger_decommit_expendable_memory_kind:
        pas_scavenger_decommit_expendable_memory();
        return;
    case pas_scavenger_decommit_bootstrap_free_heap_kind:
        pas_scavenger_decommit_bootstrap_free_heap();
        return;
    case pas_scavenger_decommit_immortal_heap_kind:
        pas_scavenger_decommit_immortal_heap();
        return;
	case pas_scavenger_decommit_verse_heap_mark_bits_kind:
		pas_scavenger_decommit_verse_heap_mark_bits();
		return;
    case pas_scavenger_decommit_free_memory_kind:
        pas_scavenger_decommit_free_memory();
        return;
    case pas_scavenger_decommit_everything_kind:
        pas_scavenger_decommit_everything();
        return;
	case pas_scavenger_do_everything_except_remote_tlcs_kind:
		pas_scavenger_do_everything_except_remote_tlcs();
		return;
    case pas_scavenger_run_synchronously_now_kind:
        pas_scavenger_run_synchronously_now();
        return;
    }
    PAS_ASSERT(!"Should not be reached");
}

void pas_scavenger_disable_shut_down(void)
{
    pas_scavenger_suspend();
    is_shut_down_enabled = false;
    pas_scavenger_resume();
}

#endif /* LIBPAS_ENABLED */
