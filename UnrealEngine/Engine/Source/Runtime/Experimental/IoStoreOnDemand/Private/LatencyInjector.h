// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Misc/Build.h"

namespace UE::IO::Private
{

#if !UE_BUILD_SHIPPING
#	define IAS_WITH_LATENCY_INJECTOR 1
#	define IAS_DISABLED_IMPL(x)
#else
#	define IAS_WITH_LATENCY_INJECTOR 0
#	define IAS_DISABLED_IMPL(x) { x; }
#endif

struct FLatencyInjector
{
	enum class EType { Network, File };
	static void Initialize(const TCHAR* CommandLine)IAS_DISABLED_IMPL(return);
	static void Set(int32 MinMs, int32 MaxMs)		IAS_DISABLED_IMPL(return);
	static void Begin(EType, uint32& Param)			IAS_DISABLED_IMPL(return);
	static bool HasExpired(uint32 Param)			IAS_DISABLED_IMPL(return true);
};

} // namespace UE::IO::Private
