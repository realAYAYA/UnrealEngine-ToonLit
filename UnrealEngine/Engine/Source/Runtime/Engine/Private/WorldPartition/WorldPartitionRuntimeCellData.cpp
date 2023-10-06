// Copyright Epic Games, Inc. All Rights Reserved.

#include "WorldPartition/WorldPartitionRuntimeCellData.h"

int32 UWorldPartitionRuntimeCellData::StreamingSourceCacheEpoch = 0;

UWorldPartitionRuntimeCellData::UWorldPartitionRuntimeCellData(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, CachedMinSourcePriority(MAX_uint8)
	, CachedSourceInfoEpoch(MIN_int32)
	, ContentBounds(ForceInit)
{}

void UWorldPartitionRuntimeCellData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Ar << DebugName;
}

void UWorldPartitionRuntimeCellData::ResetStreamingSourceInfo() const
{
	check(ShouldResetStreamingSourceInfo());
	CachedSourcePriorityWeights.Reset();
	CachedMinSourcePriority = MAX_uint8;
	CachedSourceInfoEpoch = StreamingSourceCacheEpoch;
}

void UWorldPartitionRuntimeCellData::AppendStreamingSourceInfo(const FWorldPartitionStreamingSource& Source, const FSphericalSector& SourceShape) const
{
	static_assert((uint8)EStreamingSourcePriority::Lowest == 255);
	CachedSourcePriorityWeights.Add(1.f - ((float)Source.Priority / 255.f));
	CachedMinSourcePriority = FMath::Min((uint8)Source.Priority, CachedMinSourcePriority);
}

void UWorldPartitionRuntimeCellData::MergeStreamingSourceInfo() const
{}

int32 UWorldPartitionRuntimeCellData::SortCompare(const UWorldPartitionRuntimeCellData* Other, bool bCanUseSortingCache) const
{
	// Source priority (lower value is higher prio)
	return bCanUseSortingCache ? ((int32)CachedMinSourcePriority - (int32)Other->CachedMinSourcePriority) : 0;
}

const FBox& UWorldPartitionRuntimeCellData::GetContentBounds() const
{
	return ContentBounds;
}

FBox UWorldPartitionRuntimeCellData::GetCellBounds() const
{
	return ContentBounds;
}

FString UWorldPartitionRuntimeCellData::GetDebugName() const
{
	return DebugName.GetString();
}