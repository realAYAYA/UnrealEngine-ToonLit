// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "HAL/Platform.h"
#include "HAL/PlatformTLS.h"
#include "Math/NumericLimits.h"
#include "Misc/Build.h"
#include "Trace/Config.h"

////////////////////////////////////////////////////////////////////////////////
#if !defined(UE_CALLSTACK_TRACE_ENABLED)
	#if UE_TRACE_ENABLED && !UE_BUILD_SHIPPING
		#if PLATFORM_WINDOWS && !PLATFORM_CPU_ARM_FAMILY
			#define UE_CALLSTACK_TRACE_ENABLED 1
		#endif
	#endif
#endif

#if !defined(UE_CALLSTACK_TRACE_ENABLED)
	#define UE_CALLSTACK_TRACE_ENABLED 0
#endif

////////////////////////////////////////////////////////////////////////////////
#if UE_CALLSTACK_TRACE_ENABLED

/**
 * Creates callstack tracing.
 * @param Malloc Allocator instance to use.
 */
void CallstackTrace_Create(class FMalloc* Malloc);

/**
 * Initializes callstack tracing. On some platforms this has to be delayed due to initialization order.
 */
void CallstackTrace_Initialize();

/**
 * Capture the current callstack, and trace the definition if it has not already been encountered. The returned value
 * can be used in trace events and be resolved in analysis.
 * @return Unique id identifying the current callstack.
 */
CORE_API uint32 CallstackTrace_GetCurrentId();

 /**
  * Callstack Trace Scoped Macro to avoid resolving the full callstack
  * can be used when some external libraries are not compiled with frame pointers
  * preventing us to resolve it without crashing. Instead the callstack will be 
  * only the caller address.
  */
#define CALLSTACK_TRACE_LIMIT_CALLSTACKRESOLVE_SCOPE() 						FCallStackTraceLimitResolveScope PREPROCESSOR_JOIN(FCTLMScope,__LINE__)

extern CORE_API uint32 GCallStackTracingTlsSlotIndex;

/**
* @return the fallback callstack address
*/
inline void* CallstackTrace_GetFallbackPlatformReturnAddressData()
{
	if (FPlatformTLS::IsValidTlsSlot(GCallStackTracingTlsSlotIndex))
		return FPlatformTLS::GetTlsValue(GCallStackTracingTlsSlotIndex);
	else
		return nullptr;
}

/**
* @return Needs full callstack resolve
*/
inline bool  CallstackTrace_ResolveFullCallStack()
{
	return CallstackTrace_GetFallbackPlatformReturnAddressData() == nullptr;
}

/*
 * Callstack Trace scope for override CallStack
 */
class FCallStackTraceLimitResolveScope
{
public:
	FORCENOINLINE FCallStackTraceLimitResolveScope()
	{
		if (FPlatformTLS::IsValidTlsSlot(GCallStackTracingTlsSlotIndex))
		{
			FPlatformTLS::SetTlsValue(GCallStackTracingTlsSlotIndex, PLATFORM_RETURN_ADDRESS_FOR_CALLSTACKTRACING());
		}
	}

	FORCENOINLINE ~FCallStackTraceLimitResolveScope()
	{
		if (FPlatformTLS::IsValidTlsSlot(GCallStackTracingTlsSlotIndex))
		{
			FPlatformTLS::SetTlsValue(GCallStackTracingTlsSlotIndex, nullptr);
		}
	}
};

#else // UE_CALLSTACK_TRACE_ENABLED

inline void CallstackTrace_Create(class FMalloc* Malloc) {}
inline void CallstackTrace_Initialize() {}
inline uint32 CallstackTrace_GetCurrentId() { return 0; }
inline void* CallstackTrace_GetCurrentReturnAddressData() { return nullptr; }
inline void* CallstackTrace_GetFallbackPlatformReturnAddressData() { return nullptr; }
inline bool  CallstackTrace_ResolveFullCallStack() { return true; }

#define CALLSTACK_TRACE_LIMIT_CALLSTACKRESOLVE_SCOPE()

#endif // UE_CALLSTACK_TRACE_ENABLED
