// Copyright Epic Games, Inc. All Rights Reserved.

#include "LowLevelMemoryUtils.h"
#include "CoreGlobals.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER

FORCENOINLINE void UE::Core::Private::OnInvalidLLMAllocatorNum(int32 IndexSize, int64 NewNum, SIZE_T NumBytesPerElement)
{
	UE_LOG(LogCore, Fatal, TEXT("Trying to resize TSizedLLMAllocator<%d> to an invalid size of % " INT64_FMT " with element size %" SIZE_T_FMT), IndexSize, NewNum, NumBytesPerElement);
	for (;;);
}

template void TSizedLLMAllocator<32>::ForAnyElementType::ResizeAllocation(SizeType PreviousNumElements, SizeType NumElements, SIZE_T NumBytesPerElement);
template void TSizedLLMAllocator<64>::ForAnyElementType::ResizeAllocation(SizeType PreviousNumElements, SizeType NumElements, SIZE_T NumBytesPerElement);

#endif
