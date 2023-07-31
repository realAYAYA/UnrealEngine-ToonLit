// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/LowLevelMemTracker.h"

#if ENABLE_LOW_LEVEL_MEM_TRACKER
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/Set.h"
#include "Containers/SortedMap.h"
#include "LowLevelMemoryUtils.h"

// Private defines configuring LLM; see also public defines in LowLevelMemTracker.h
// LLM_ALLOW_NAMES_TAGS: Set to 1 to allow arbitrary FNames to be used as tags, at the cost of more LLM memory usage per allocation. Set to 0 to store only the top-level ELLMTag for each allocation.
// LLM_SCOPES always use FNames for the definition of their tag
// Storing these tags on each allocation however requires 4 bytes per allocation
// To reduce the memory overhead of LLM, this can be reduced to 1 byte per allocation at the cost of showing only the top-level 256 Tags; tags are replaced with their containing toplevel tag during allocation.
// This setting is ignored if features requiring more information per allocation (LLM_ALLOW_STATS, LLM_ALLOW_ASSETS_TAGS) are enabled
#ifndef LLM_ALLOW_NAMES_TAGS
	#define LLM_ALLOW_NAMES_TAGS 1
#endif

// Enable storing the full tag for each alloc if (1) arbitrary names tags are allowed or (2) Stats are allowed (stats use names tags) or (3) Asset tags are allowed (assets use names tags)
#define LLM_ENABLED_FULL_TAGS LLM_ALLOW_NAMES_TAGS || LLM_ALLOW_STATS || LLM_ALLOW_ASSETS_TAGS

// Whether to enable running with reduced threads. This is currently enabled because the engine crashes with -norenderthread
#define LLM_ENABLED_REDUCE_THREADS 0

// this controls if the commandline is used to enable tracking, or to disable it. If LLM_COMMANDLINE_ENABLES_FUNCTIONALITY is true, 
// then tracking will only happen through Engine::Init(), at which point it will be disabled unless the commandline tells 
// it to keep going (with -llm). If LLM_COMMANDLINE_ENABLES_FUNCTIONALITY is false, then tracking will be on unless the commandline
// disables it (with -nollm)
#ifndef LLM_COMMANDLINE_ENABLES_FUNCTIONALITY
	#define LLM_COMMANDLINE_ENABLES_FUNCTIONALITY 1
#endif 

// when set to 1, forces LLM to be enabled without having to parse the command line.
#ifndef LLM_AUTO_ENABLE
	#define LLM_AUTO_ENABLE 0
#endif

// There is a little memory and cpu overhead in tracking peak memory but it is generally more useful than current memory.
// Disable if you need a little more memory or speed
#define LLM_ENABLED_TRACK_PEAK_MEMORY 1



namespace UE
{
namespace LLMPrivate
{
	/**
	 * FTagData: Description of the properties of a Tag that can be used in LLM_SCOPE
	 */
	class FTagData
	{
	public:

		FTagData(FName InName, FName InDisplayName, FName InParentName, FName InStatName, FName InSummaryStatName, bool bInHasEnumTag, ELLMTag InEnumTag, ETagReferenceSource InReferenceSource);
		FTagData(FName InName, FName InDisplayName, const FTagData* InParent, FName InStatName, FName InSummaryStatName, bool bInHasEnumTag, ELLMTag InEnumTag, ETagReferenceSource InReferenceSource);
		~FTagData();

		bool IsParentConstructed() const;
		bool IsFinishConstructed() const;
		FName GetName() const;
		FName GetDisplayName() const;
		FString GetDisplayPath() const;
		void AppendDisplayPath(FStringBuilderBase& Result) const;
		const FTagData* GetParent() const;
		FName GetParentName() const;
		FName GetStatName() const;
		FName GetSummaryStatName() const;
		ELLMTag GetEnumTag() const;
		bool HasEnumTag() const;
		const FTagData* GetContainingEnumTagData() const;
		ELLMTag GetContainingEnum() const;
		ETagReferenceSource GetReferenceSource() const;
		int32 GetIndex() const;
		bool IsReportable() const;

		void SetParent(const FTagData* InParent);
		void SetIndex(int32 InIndex);
		void SetIsReportable(bool bInReportable);
		void SetFinishConstructed();

		// These functions are normally invalid - these properties should be immutable - but are called for EnumTags during bootstrapping
		FTagData(ELLMTag InEnumTag);
		void SetName(FName InName);
		void SetDisplayName(FName InDisplayName);
		void SetStatName(FName InStatName);
		void SetSummaryStatName(FName InSummaryStatName);
		void SetParentName(FName InParentName);

	private:
		bool IsUsedAsDisplayParent() const;

		FName Name;
		FName DisplayName;
		union
		{
			const FTagData* Parent;
			FName ParentName;
		};
		FName StatName;
		FName SummaryStatName;
		int32 Index;
		ELLMTag EnumTag;
		ETagReferenceSource ReferenceSource;
		bool bIsFinishConstructed;
		bool bParentIsName;
		bool bHasEnumTag;
		bool bIsReportable;
	};

	// TagData container types that use FLLMAllocator
	class FTagDataNameMap : public TMap<FName, FTagData*, FDefaultSetLLMAllocator>
	{
		using TMap<FName, FTagData*, FDefaultSetLLMAllocator>::TMap;
	};
	class FConstTagDataArray : public TArray<const FTagData*, FDefaultLLMAllocator>
	{
		using TArray<const FTagData*, FDefaultLLMAllocator>::TArray;
	};
	class FTagDataArray : public TArray<FTagData*, FDefaultLLMAllocator>
	{
		using TArray<FTagData*, FDefaultLLMAllocator>::TArray;
	};
}
}

// Interface for Algo::TopologicalSort for FTagDataArray
inline UE::LLMPrivate::FTagData** GetData(UE::LLMPrivate::FTagDataArray& Array)
{
	return Array.GetData();
}
inline UE::LLMPrivate::FTagDataArray::SizeType GetNum(UE::LLMPrivate::FTagDataArray& Array)
{
	return Array.Num();
}

namespace UE
{
namespace LLMPrivate
{
	/** Size information stored on the tracker for a tag; includes amounts aggregated from threadstates and from external api users */
	struct FTrackerTagSizeData
	{
		int64 Size = 0;
#if LLM_ENABLED_TRACK_PEAK_MEMORY
		int64 PeakSize = 0;
#endif
		int64 ExternalAmount = 0;
		bool bExternalValid = false;
		bool bExternalAddToTotal = false;

		int64 GetSize(bool bTrackPeaks) const
		{
#if LLM_ENABLED_TRACK_PEAK_MEMORY
			if (bTrackPeaks)
			{
				return PeakSize;
			}
			else
#endif
			{
				return Size;
			}
		};
	};
	typedef TFastPointerLLMMap<const FTagData*, FTrackerTagSizeData> FTrackerTagSizeMap;

	/**
	 * Size information stored on each threadstate for a tag
	 * TagSizes are sorted by Index instead of by pointer in the ThreadTagSizeMap to enforce the constraint that Parents come before children
	 */
	struct FThreadTagSizeData
	{
		const FTagData* TagData = nullptr;
		int64 Size = 0;
	};
	// 
	typedef TSortedMap<int32, FThreadTagSizeData, FDefaultLLMAllocator> FThreadTagSizeMap;
}
}

FName LLMGetTagUniqueName(ELLMTag Tag);

#endif		// #if ENABLE_LOW_LEVEL_MEM_TRACKER
