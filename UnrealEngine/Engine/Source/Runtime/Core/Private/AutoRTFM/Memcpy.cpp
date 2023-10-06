// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)
#include "Memcpy.h"
#include "ContextInlines.h"

namespace AutoRTFM
{

void* MemcpyToNew(void* InDst, const void* InSrc, size_t Size, FContext* Context)
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("MemcpyToNew(%p, %p, %zu)"), InDst, InSrc, Size);
    AutoRTFM::Unreachable();
}

void* Memcpy(void* InDst, const void* InSrc, size_t Size, FContext* Context)
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("Memcpy(%p, %p, %zu)"), InDst, InSrc, Size);
    Context->RecordWrite(InDst, Size);
    return memcpy(InDst, InSrc, Size);
}

void* Memmove(void* InDst, const void* InSrc, size_t Size, FContext* Context)
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("Memmove(%p, %p, %zu)"), InDst, InSrc, Size);
	Context->RecordWrite(InDst, Size);
	return memmove(InDst, InSrc, Size);
}

void* Memset(void* InDst, int Value, size_t Size, FContext* Context)
{
	UE_LOG(LogAutoRTFM, Verbose, TEXT("Memset(%p, %d, %zu)"), InDst, Value, Size);
	Context->RecordWrite(InDst, Size);
	return memset(InDst, Value, Size);
}

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
