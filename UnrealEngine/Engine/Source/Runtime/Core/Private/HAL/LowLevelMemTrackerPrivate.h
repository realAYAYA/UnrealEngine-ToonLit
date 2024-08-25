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
#define LLM_ENABLED_FULL_TAGS (LLM_ALLOW_NAMES_TAGS || LLM_ALLOW_STATS || LLM_ALLOW_ASSETS_TAGS)

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
	struct FTagDataNameKey
	{
		FName Name;
		ELLMTagSet TagSet;
		
		FTagDataNameKey(FName InName, ELLMTagSet InTagSet) :
			Name(InName),
			TagSet(InTagSet)
		{
		}

		friend uint32 GetTypeHash(const FTagDataNameKey& Key)
		{
			constexpr uint32 HashPrime = 101;
			return GetTypeHash(Key.Name) + HashPrime * static_cast<uint32>(Key.TagSet);
		}

		friend bool operator==(const FTagDataNameKey& A, const FTagDataNameKey& B)
		{
			return A.Name == B.Name && A.TagSet == B.TagSet;
		}

		friend bool operator!=(const FTagDataNameKey& A, const FTagDataNameKey& B)
		{
			return (A.Name != B.Name || A.TagSet != B.TagSet);
		}

		FTagDataNameKey() = delete;
	};

	/**
	 * FTagData: Description of the properties of a Tag that can be used in LLM_SCOPE
	 */
	class FTagData
	{
	public:

		FTagData(FName InName, ELLMTagSet InTagSet, FName InDisplayName, FName InParentName, FName InStatName,
			FName InSummaryStatName, bool bInHasEnumTag, ELLMTag InEnumTag, ETagReferenceSource InReferenceSource);
		FTagData(FName InName, ELLMTagSet InTagSet, FName InDisplayName, const FTagData* InParent, FName InStatName,
			FName InSummaryStatName, bool bInHasEnumTag, ELLMTag InEnumTag, ETagReferenceSource InReferenceSource);
		~FTagData();

		bool IsParentConstructed() const;
		bool IsFinishConstructed() const;
		FName GetName() const;
		FName GetDisplayName() const;
		void GetDisplayPath(FStringBuilderBase& Result, int32 MaxLen=-1) const;
		void AppendDisplayPath(FStringBuilderBase& Result, int32 MaxLen=-1) const;
		const FTagData* GetParent() const;
		FName GetParentName() const;
		FName GetParentNameSafeBeforeFinishConstruct() const;
		FName GetStatName() const;
		FName GetSummaryStatName() const;
		ELLMTag GetEnumTag() const;
		ELLMTagSet GetTagSet() const;
		bool HasEnumTag() const;
		const FTagData* GetContainingEnumTagData() const;
		ELLMTag GetContainingEnum() const;
		ETagReferenceSource GetReferenceSource() const;
		int32 GetIndex() const;
		bool IsReportable() const;
		bool IsStatsReportable() const;

		void SetParent(const FTagData* InParent);
		void SetIndex(int32 InIndex);
		void SetIsReportable(bool bInReportable);
		void SetIsStatsReportable(bool bInReportable);
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
		ELLMTagSet TagSet;
		bool bIsFinishConstructed;
		bool bParentIsName;
		bool bHasEnumTag;
		bool bIsReportable;
	};

	// TagData container types that use FLLMAllocator
	class FTagDataNameMap : public TMap<FTagDataNameKey, FTagData*, FDefaultSetLLMAllocator>
	{
		using TMap<FTagDataNameKey, FTagData*, FDefaultSetLLMAllocator>::TMap;
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
		int64 SizeInSnapshot = 0;
		int64 ExternalAmount = 0;
		bool bExternalValid = false;
		bool bExternalAddToTotal = false;

		int64 GetSize(UE::LLM::ESizeParams SizeParams) const
		{
			int64 CurrentSize = Size;

#if LLM_ENABLED_TRACK_PEAK_MEMORY
			if (EnumHasAnyFlags(SizeParams, UE::LLM::ESizeParams::ReportPeak))
			{
				CurrentSize = PeakSize;
			}
#endif
			// Note, this will also subtract the snapshotted size from PeakSize if that flag is enabled
			if (EnumHasAnyFlags(SizeParams, UE::LLM::ESizeParams::RelativeToSnapshot))
			{
				CurrentSize = FMath::Clamp<int64>(CurrentSize - SizeInSnapshot, 0, INT64_MAX);
			}

			return CurrentSize;
		};

		void CaptureSnapshot()
		{
			SizeInSnapshot = Size;
		}

		void ClearSnapshot()
		{
			SizeInSnapshot = 0;
		}
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
