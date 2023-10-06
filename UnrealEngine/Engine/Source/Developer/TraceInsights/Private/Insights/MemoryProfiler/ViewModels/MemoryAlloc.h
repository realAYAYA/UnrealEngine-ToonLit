// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "TraceServices/Model/AllocationsProvider.h"
#include "TraceServices/Model/Callstack.h"

namespace Insights
{

////////////////////////////////////////////////////////////////////////////////////////////////////

class FMemoryAlloc
{
	friend class SMemAllocTableTreeView;

public:
	FMemoryAlloc();
	~FMemoryAlloc();

	int64 GetStartEventIndex() const { return int64(StartEventIndex); }
	int64 GetEndEventIndex() const { return int64(EndEventIndex); }
	int64 GetEventDistance() const { return EndEventIndex == ~0 ? int64(EndEventIndex) : int64(EndEventIndex) - int64(StartEventIndex); }
	double GetStartTime() const { return StartTime; }
	double GetEndTime() const { return EndTime; }
	double GetDuration() const { return EndTime - StartTime; }
	uint64 GetAddress() const { return Address; }
	uint64 GetPage() const { return Address & ~(4llu*1024-1); }
	int64 GetSize() const { return Size; }
	bool IsHeap() const { return bIsHeap; }
	TraceServices::TagIdType GetTagId() const { return TagId; }
	const TCHAR* GetTag() const { return Tag; }
	const TCHAR* GetAsset() const { return Asset; }
	const TCHAR* GetPackage() const { return Package; }
	const TCHAR* GetClassName() const { return ClassName; }
	uint32 GetCallstackId() const { return CallstackId; }
	uint32 GetFreeCallstackId() const { return FreeCallstackId; }
	const TraceServices::FCallstack* GetCallstack() const { return Callstack; }
	const TraceServices::FCallstack* GetFreeCallstack() const { return FreeCallstack; }
	FText GetFullCallstack() const;
	FText GetFullCallstackSourceFiles() const;
	HeapId GetRootHeap() const { return RootHeap; }

private:
	uint32 StartEventIndex;
	uint32 EndEventIndex;
	double StartTime;
	double EndTime;
	uint64 Address;
	int64 Size;
	TraceServices::TagIdType TagId;
	const TCHAR* Tag;
	const TCHAR* Asset;
	const TCHAR* Package;
	const TCHAR* ClassName;
	uint32 CallstackId;
	uint32 FreeCallstackId;
	const TraceServices::FCallstack* Callstack;
	const TraceServices::FCallstack* FreeCallstack;
	HeapId RootHeap;
	bool bIsHeap;
	bool bIsDecline;
};

////////////////////////////////////////////////////////////////////////////////////////////////////

} // namespace Insights
