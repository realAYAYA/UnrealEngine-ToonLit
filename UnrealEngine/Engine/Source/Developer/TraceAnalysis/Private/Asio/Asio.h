// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"

#if PLATFORM_WINDOWS
#	include "Windows/AllowWindowsPlatformTypes.h"
#	include "Windows/AllowWindowsPlatformAtomics.h"
#endif

THIRD_PARTY_INCLUDES_START

namespace asio {
namespace detail {

template <typename ExceptionType>
void throw_exception(const ExceptionType& Exception)
{
	/* Intentionally blank */
}

} // namespace detail
} // namespace asio

#include "asio/connect.hpp"
#include "asio/error.hpp"
#include "asio/io_context.hpp"
#include "asio/ip/tcp.hpp"
#include "asio/read.hpp"
#include "asio/write.hpp"
#include "asio/write_at.hpp"

#if PLATFORM_WINDOWS
#	include "asio/windows/random_access_handle.hpp"
#	include "asio/windows/object_handle.hpp"
#endif

#if PLATFORM_WINDOWS
#	include <winsock2.h>
#	pragma comment(lib, "ws2_32.lib")
#endif

THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#	include "Windows/HideWindowsPlatformAtomics.h"
#	include "Windows/HideWindowsPlatformTypes.h"
#endif
