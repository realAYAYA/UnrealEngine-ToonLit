// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/EnumClassFlags.h"
#include "ProfilingDebugging/MemoryTrace.h" // for EMemoryTraceHeapAllocationFlags
#include "TraceServices/Model/AllocationsProvider.h" // for TagIdType

namespace TraceServices
{

struct FAllocationItem
{
	static constexpr uint32 AlignmentBits = 8;
	static constexpr uint32 AlignmentShift = 56;
	static constexpr uint64 SizeMask = (1ULL << AlignmentShift) - 1;

	static uint64 UnpackSize(uint64 SizeAndAlignment) { return (SizeAndAlignment & SizeMask); }
	static uint32 UnpackAlignment(uint64 SizeAndAlignment) { return static_cast<uint32>(SizeAndAlignment >> AlignmentShift); }
	static uint64 PackSizeAndAlignment(uint64 Size, uint8 Alignment) { return (Size | (static_cast<uint64>(Alignment) << AlignmentShift)); }

	FORCEINLINE bool IsContained(uint64 InAddress) const { return Address >= InAddress && Address < (Address + GetSize()); }
	FORCEINLINE uint64 GetEndAddress() const { return Address + UnpackSize(SizeAndAlignment); }
	FORCEINLINE uint64 GetSize() const { return UnpackSize(SizeAndAlignment); }
	FORCEINLINE uint32 GetAlignment() const { return UnpackAlignment(SizeAndAlignment); }
	FORCEINLINE bool IsHeap() const { return EnumHasAnyFlags(Flags, EMemoryTraceHeapAllocationFlags::Heap); }

	uint64 Address;
	uint64 SizeAndAlignment; // (Alignment << AlignmentShift) | Size
	uint32 StartEventIndex;
	uint32 EndEventIndex;
	double StartTime;
	double EndTime;
	uint16 AllocThreadId;
	uint16 FreeThreadId;
	uint32 AllocCallstackId;
	uint32 FreeCallstackId;
	uint32 MetadataId;
	TagIdType Tag; // uint32
	uint8 RootHeap;
	EMemoryTraceHeapAllocationFlags Flags; // uint8
};

static_assert(sizeof(FAllocationItem) == 64, "struct FAllocationItem needs packing");

} // namespace TraceServices
