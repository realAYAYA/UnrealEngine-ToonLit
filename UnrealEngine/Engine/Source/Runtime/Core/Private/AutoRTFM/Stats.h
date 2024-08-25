// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Toggles.h"
#include "Utils.h"

namespace AutoRTFM
{

enum class EStatsKind : uint8_t
{
	Transaction = 0,
	Commit,
	Abort,
	AverageTransactionDepth,
	MaximumTransactionDepth,
	AverageWriteLogEntries,
	MaximumWriteLogEntries,
	AverageWriteLogBytes,
	MaximumWriteLogBytes,
	HitSetHit,
	HitSetMiss,
	HitSetSkippedBecauseOfStackLocalMemory,
	AverageCommitTasks,
	MaximumCommitTasks,
	AverageAbortTasks,
	MaximumAbortTasks,
	NewMemoryTrackerHit,
	NewMemoryTrackerMiss,
	AverageHitSetSize,
	AverageHitSetCapacity,
	Total
};

template<EStatsKind Kind> constexpr bool IsStatsKindNoArgs()
{
	switch (Kind)
	{
	default:
		return false;
	case EStatsKind::Transaction:
	case EStatsKind::Commit:
	case EStatsKind::Abort:
	case EStatsKind::HitSetHit:
	case EStatsKind::HitSetMiss:
	case EStatsKind::HitSetSkippedBecauseOfStackLocalMemory:
	case EStatsKind::NewMemoryTrackerHit:
	case EStatsKind::NewMemoryTrackerMiss:
		return true;
	}
}

template<EStatsKind Kind> constexpr bool IsStatsKindOneArg()
{
	switch (Kind)
	{
	default:
		return false;
	case EStatsKind::AverageTransactionDepth:
	case EStatsKind::MaximumTransactionDepth:
	case EStatsKind::AverageWriteLogEntries:
	case EStatsKind::MaximumWriteLogEntries:
	case EStatsKind::AverageWriteLogBytes:
	case EStatsKind::MaximumWriteLogBytes:
	case EStatsKind::AverageCommitTasks:
	case EStatsKind::MaximumCommitTasks:
	case EStatsKind::AverageAbortTasks:
	case EStatsKind::MaximumAbortTasks:
	case EStatsKind::AverageHitSetSize:
	case EStatsKind::AverageHitSetCapacity:
		return true;
	}
}

template<EStatsKind Kind> constexpr bool IsStatsKindAverage()
{
	switch (Kind)
	{
	default:
		return false;
	case EStatsKind::AverageTransactionDepth:
	case EStatsKind::AverageWriteLogEntries:
	case EStatsKind::AverageWriteLogBytes:
	case EStatsKind::AverageCommitTasks:
	case EStatsKind::AverageAbortTasks:
	case EStatsKind::AverageHitSetSize:
	case EStatsKind::AverageHitSetCapacity:
		return true;
	}
}

template<EStatsKind Kind> constexpr bool IsStatsKindMaximum()
{
	switch (Kind)
	{
	default:
		return false;
	case EStatsKind::MaximumTransactionDepth:
	case EStatsKind::MaximumWriteLogEntries:
	case EStatsKind::MaximumWriteLogBytes:
	case EStatsKind::MaximumCommitTasks:
	case EStatsKind::MaximumAbortTasks:
		return true;
	}
}

struct FStats final
{
	FStats()
	{
		FMemory::Memset(Datas, 0, sizeof(Datas));
	}

    ~FStats()
    {
		if constexpr (bCollectStats)
		{
			Report();
		}
    }

    void Report() const;

	template<EStatsKind Kind> UE_AUTORTFM_FORCEINLINE void Collect()
	{
		static_assert(IsStatsKindNoArgs<Kind>());

		if constexpr (bCollectStats)
		{
			Datas[static_cast<size_t>(Kind)] += 1;
		}
	}

	template<EStatsKind Kind> UE_AUTORTFM_FORCEINLINE void Collect(uint64_t Data)
	{
		static_assert(IsStatsKindOneArg<Kind>());

		if constexpr (bCollectStats)
		{
			if constexpr (IsStatsKindAverage<Kind>())
			{
				Datas[static_cast<size_t>(Kind)] += Data;
			}
			else if constexpr (IsStatsKindMaximum<Kind>())
			{
				uint64_t* const Ptr = &Datas[static_cast<size_t>(Kind)];
				*Ptr = FMath::Max(*Ptr, Data);
			}
		}
	}

private:
	uint64_t Datas[static_cast<size_t>(EStatsKind::Total)];

	template<EStatsKind Kind> void Report(const uint64_t Data) const;
};

extern FStats Stats;

template<typename T, bool bActive = bCollectStats> struct TStatStorage;

template<typename T> struct TStatStorage<T, false> final
{
	UE_AUTORTFM_FORCEINLINE TStatStorage(const T) { /* ignored */ }
	UE_AUTORTFM_FORCEINLINE void operator=(const T) { /* ignored */ }
	UE_AUTORTFM_FORCEINLINE void operator+=(const T) { /* ignored */ }
	UE_AUTORTFM_FORCEINLINE operator T() const { return T(0); }
};

template<typename T> struct TStatStorage<T, true> final
{
	UE_AUTORTFM_FORCEINLINE TStatStorage(const T t) : Data(t) {}
	UE_AUTORTFM_FORCEINLINE void operator=(const T t) { Data = t; }
	UE_AUTORTFM_FORCEINLINE void operator+=(const T t) { Data += t; }
	UE_AUTORTFM_FORCEINLINE operator T() const { return Data; }
private:
	T Data;
};

} // namespace AutoRTFM
