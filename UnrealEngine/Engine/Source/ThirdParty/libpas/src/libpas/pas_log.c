/*
 * Copyright (c) 2019 Apple Inc. All rights reserved.
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

#include "pas_log.h"

#include <errno.h>
#include "pas_snprintf.h"
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stddef.h>

pas_system_thread_id pas_thread_that_is_crash_logging;

// Debug option to log to a file instead of stdout by default.
// This does not affect pas_fd_stream.
#define PAS_DEBUG_LOG_TO_SYSLOG 0

#if PAS_DEBUG_LOG_TO_SYSLOG
#include <sys/syslog.h>
#endif

void pas_vlog_fd(int fd, const char* format, va_list list)
{
    char buf[PAS_LOG_MAX_BYTES];
    ptrdiff_t result;
    char* ptr;
    size_t bytes_left_to_write;
    pas_system_thread_id thread_that_is_crash_logging;

    thread_that_is_crash_logging = pas_thread_that_is_crash_logging;
    while (thread_that_is_crash_logging && thread_that_is_crash_logging != pas_get_current_system_thread_id()) {
        pas_compiler_fence();
        thread_that_is_crash_logging = pas_thread_that_is_crash_logging;
    }

    result = pas_vsnprintf(buf, PAS_LOG_MAX_BYTES, format, list);

    PAS_ASSERT(result >= 0);

    if ((size_t)result < PAS_LOG_MAX_BYTES)
        bytes_left_to_write = (size_t)result;
    else
        bytes_left_to_write = PAS_LOG_MAX_BYTES - 1;

    ptr = buf;

    while (bytes_left_to_write) {
#ifdef _WIN32
        result = _write(fd, ptr, bytes_left_to_write);
#else
        result = write(fd, ptr, bytes_left_to_write);
#endif
        if (result < 0) {
            PAS_ASSERT(errno == EINTR);
            continue;
        }

        PAS_ASSERT(result);

        ptr += result;
        bytes_left_to_write -= (size_t)result;
    }
}

void pas_log_fd(int fd, const char* format, ...)
{
    va_list arg_list;
    va_start(arg_list, format);
    pas_vlog_fd(fd, format, arg_list);
    va_end(arg_list);
}

void pas_vlog(const char* format, va_list list)
{
#if PAS_DEBUG_LOG_TO_SYSLOG
    PAS_IGNORE_WARNINGS_BEGIN("format-nonliteral");
    syslog(LOG_WARNING, format, list);
    PAS_IGNORE_WARNINGS_END;
#else
    pas_vlog_fd(PAS_LOG_DEFAULT_FD, format, list);
#endif
}

void pas_log(const char* format, ...)
{
    va_list arg_list;
    va_start(arg_list, format);
    pas_vlog(format, arg_list);
    va_end(arg_list);
}

void pas_start_crash_logging(void)
{
    pas_thread_that_is_crash_logging = pas_get_current_system_thread_id();
    pas_fence();
}

#endif /* LIBPAS_ENABLED */
