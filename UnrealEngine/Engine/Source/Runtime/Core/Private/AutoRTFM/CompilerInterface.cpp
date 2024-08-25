// Copyright Epic Games, Inc. All Rights Reserved.

#if (defined(__AUTORTFM) && __AUTORTFM)

#include "ContextInlines.h"
#include "FunctionMapInlines.h"
#include "Memcpy.h"

namespace AutoRTFM
{

void AbortDueToBadAlignment(FContext* Context, void* Ptr, size_t Alignment, const char* Message = nullptr)
{
    Context->DumpState();
    fprintf(stderr, "Aborting because alignment error: expected alignment %zu, got pointer %p.\n", Alignment, Ptr);
    if (Message)
    {
        fprintf(stderr, "%s\n", Message);
    }
    abort();
}

void CheckAlignment(FContext* Context, void* Ptr, size_t AlignmentMask)
{
    if (reinterpret_cast<uintptr_t>(Ptr) & AlignmentMask)
    {
        AbortDueToBadAlignment(Context, Ptr, AlignmentMask + 1);
    }
}

extern "C" UE_AUTORTFM_API AUTORTFM_NO_ASAN void autortfm_record_write(void* Ptr, size_t Size)
{
	// check for writes to null here so we end up crashing in the user
	// code rather than in the autortfm runtime.
	if (UNLIKELY(nullptr == Ptr))
	{
		return;
	}

	FContext* Context = FContext::Get();
	Context->RecordWrite(Ptr, Size);
}

extern "C" UE_AUTORTFM_API AUTORTFM_NO_ASAN void autortfm_record_write_8(void* Ptr)
{
	// check for writes to null here so we end up crashing in the user
	// code rather than in the autortfm runtime.
	if (UNLIKELY(nullptr == Ptr))
	{
		return;
	}

	FContext* Context = FContext::Get();
	Context->RecordWrite<8>(Ptr);
}

extern "C" UE_AUTORTFM_API void* autortfm_lookup_function(void* OriginalFunction, const char* Where)
{
	FContext* Context = FContext::Get();
    return FunctionMapLookup(OriginalFunction, Where);
}

extern "C" UE_AUTORTFM_API void autortfm_memcpy(void* Dst, const void* Src, size_t Size)
{
	FContext* Context = FContext::Get();
    Memcpy(Dst, Src, Size, Context);
}

extern "C" UE_AUTORTFM_API void autortfm_memmove(void* Dst, const void* Src, size_t Size)
{
	FContext* Context = FContext::Get();
    Memmove(Dst, Src, Size, Context);
}

extern "C" UE_AUTORTFM_API void autortfm_memset(void* Dst, int Value, size_t Size)
{
	FContext* Context = FContext::Get();
    Memset(Dst, Value, Size, Context);
}

extern "C" UE_AUTORTFM_API void autortfm_llvm_fail(const char* Message)
{
    if (Message)
    {
		UE_LOG(LogAutoRTFM, Fatal, TEXT("Transaction failing because of language issue '%s'."), ANSI_TO_TCHAR(Message));
    }
    else
    {
		UE_LOG(LogAutoRTFM, Fatal, TEXT("Transaction failing because of language issue."));
	}

	FContext* Context = FContext::Get();
    Context->AbortByLanguageAndThrow();
}

extern "C" UE_AUTORTFM_API void autortfm_llvm_alignment_error(FContext* Context, void* Ptr, size_t Alignment, const char* Message)
{
    AbortDueToBadAlignment(Context, Ptr, Alignment, Message);
}

extern "C" UE_AUTORTFM_API void autortfm_llvm_error(const char* Message)
{
	if (Message)
	{
		UE_LOG(LogAutoRTFM, Fatal, TEXT("Transaction failing because of LLVM issue '%s'."), ANSI_TO_TCHAR(Message));
	}
	else
	{
		UE_LOG(LogAutoRTFM, Fatal, TEXT("Transaction failing because of LLVM issue."));
	}
}

} // namespace AutoRTFM

#endif // defined(__AUTORTFM) && __AUTORTFM
