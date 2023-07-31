// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RigHierarchyDefines.h"
#include "RigHierarchyCache.h"
#include "RigHierarchyPose.generated.h"

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigPoseElement
{
public:

	GENERATED_BODY()

	FRigPoseElement()
	: Index()
	, GlobalTransform(FTransform::Identity)
	, LocalTransform(FTransform::Identity)
	, CurveValue(0.f)
	{
	}

	UPROPERTY()
	FCachedRigElement Index;

	UPROPERTY()
	FTransform GlobalTransform;

	UPROPERTY()
	FTransform LocalTransform;

	UPROPERTY()
	float CurveValue;
};

USTRUCT(BlueprintType)
struct CONTROLRIG_API FRigPose
{
public:

	GENERATED_BODY()

	FRigPose()
	: HierarchyTopologyVersion(INDEX_NONE)
	, PoseHash(INDEX_NONE)
	, CachedPoseHash(INDEX_NONE)
	{
	}

	FORCEINLINE void Reset() { Elements.Reset(); }

	FORCEINLINE int32 Num() const { return Elements.Num(); }
	FORCEINLINE bool IsValidIndex(int32 InIndex) const { return Elements.IsValidIndex(InIndex); }

	FORCEINLINE int32 GetIndex(const FRigElementKey& InKey) const
	{
		if(CachedPoseHash != PoseHash)
		{
			KeyToIndex.Reset();
			CachedPoseHash = PoseHash;
		}

		if(const int32* IndexPtr = KeyToIndex.Find(InKey))
		{
			return *IndexPtr;
		}
		
		for(int32 Index=0;Index<Elements.Num();Index++)
		{
			const FRigElementKey& Key = Elements[Index].Index.GetKey();

			KeyToIndex.Add(Key, Index);
			if(Key == InKey)
			{
				return Index;
			}
		}
		return INDEX_NONE;
	}

	FORCEINLINE bool Contains(const FRigElementKey& InKey) const
	{
		return GetIndex(InKey) != INDEX_NONE;
	}

	FORCEINLINE const FRigPoseElement& operator[](int32 InIndex) const { return Elements[InIndex]; }
	FORCEINLINE FRigPoseElement& operator[](int32 InIndex) { return Elements[InIndex]; }
	FORCEINLINE TArray<FRigPoseElement>::RangedForIteratorType      begin()       { return Elements.begin(); }
	FORCEINLINE TArray<FRigPoseElement>::RangedForConstIteratorType begin() const { return Elements.begin(); }
	FORCEINLINE TArray<FRigPoseElement>::RangedForIteratorType      end()         { return Elements.end();   }
	FORCEINLINE TArray<FRigPoseElement>::RangedForConstIteratorType end() const   { return Elements.end();   }

	UPROPERTY()
	TArray<FRigPoseElement> Elements;

	UPROPERTY()
	int32 HierarchyTopologyVersion;

	UPROPERTY()
	int32 PoseHash;

private:
	
	mutable int32 CachedPoseHash;
	mutable TMap<FRigElementKey, int32> KeyToIndex;
};
