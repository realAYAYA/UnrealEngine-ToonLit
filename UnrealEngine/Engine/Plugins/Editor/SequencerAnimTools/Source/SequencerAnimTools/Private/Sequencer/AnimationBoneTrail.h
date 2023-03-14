// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Trail.h"
#include "TrajectoryCache.h"
#include "TrajectoryDrawInfo.h"

#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Animation/AnimSequence.h"

namespace UE 
{
namespace SequencerAnimTools	
{

class FAnimTrajectoryCache : public FGCObject
{
public:
	FAnimTrajectoryCache(USkeletalMeshComponent* InSkeletalMeshComponent, TWeakPtr<class ISequencer> InWeakSequencer)
		: WeakSequencer(InWeakSequencer)
		, SkeletalMeshComponent(InSkeletalMeshComponent)
		, CachedAnimSequence(NewObject<UAnimSequence>())
		, GlobalBoneTransforms()
		, ComponentBoneTransforms()
		, SkelToTrackIdx()
		, AnimRange()
		, Spacing()
		, bDirty(true)
	{
		CachedAnimSequence->SetSkeleton(InSkeletalMeshComponent->GetSkeletalMeshAsset()->GetSkeleton());
	}

	void Evaluate(FTrajectoryCache* ParentTrajectoryCache);
	void UpdateRange(const TRange<double>& EvalRange, FTrajectoryCache* ParentTrajectoryCache, const int32 BoneIdx);
	const TRange<double>& GetRange() const { return AnimRange; }
	USkeletalMeshComponent* GetSkeletalMeshComponent() const { return SkeletalMeshComponent.Get(); }

	void MarkAsDirty() { bDirty = true; }
	bool IsDirty() const { return bDirty; }

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("UE::SequencerAnimTools::FAnimTrajectoryCache");
	}

private:

	void GetSpaceBasedAnimationData(TArray<TArray<FTransform>>& OutAnimationDataInComponentSpace);

	TWeakPtr<ISequencer> WeakSequencer;
	TWeakObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;
	UAnimSequence* CachedAnimSequence;
	TArray<TArray<FTransform>> GlobalBoneTransforms;
	TArray<TArray<FTransform>> ComponentBoneTransforms;
	TArray<int32> SkelToTrackIdx;
	TRange<double> AnimRange;
	double Spacing;
	bool bDirty;

	friend class FAnimBoneTrajectoryCache;
};

class FAnimBoneTrajectoryCache : public FTrajectoryCache
{
public:
	FAnimBoneTrajectoryCache(const FName& BoneName, TSharedPtr<FAnimTrajectoryCache> InAnimTrajectoryCache)
		: AnimTrajectoryCache(InAnimTrajectoryCache)
		, BoneIdx(InAnimTrajectoryCache->CachedAnimSequence->GetSkeleton()->GetReferenceSkeleton().FindBoneIndex(BoneName))
	{}

	// begin FTrajectoryCache interface
	virtual const FTransform& Get(const double InTime) const override 
	{ 
		return AnimTrajectoryCache->GlobalBoneTransforms[BoneIdx][FMath::Clamp(int32((InTime - AnimTrajectoryCache->AnimRange.GetLowerBoundValue()) / AnimTrajectoryCache->Spacing), 0, AnimTrajectoryCache->ComponentBoneTransforms[0].Num() - 1)];
	}

	virtual FTransform GetInterp(const double InTime) const override;

	// This cache is read-only
	virtual void Set(const double InTime, const FTransform& InValue) override {};
	virtual void SetTransforms(TArray <FTransform>& InTransforms, TArray<FTransform>& InParentTransforms) override {};
	virtual TArray<double> GetAllTimesInRange(const TRange<double>& InRange) const override;
	// end FTrajectoryCache interface

	// TODO: true for now
	bool IsValid() const { return true; }
	TSharedPtr<FAnimTrajectoryCache> GetAnimCache() const { return AnimTrajectoryCache; }
	int32 GetBoneIndex() const { return BoneIdx; }

private:

	TSharedPtr<FAnimTrajectoryCache> AnimTrajectoryCache;
	int32 BoneIdx;
};

class FAnimationBoneTrail : public FTrail 
{
public:
	FAnimationBoneTrail(USceneComponent* InOwner, const FLinearColor& InColor, const bool bInIsVisible, TSharedPtr<FAnimTrajectoryCache> InAnimTrajectoryCache, const FName& InBoneName, const bool bInIsRootBone)
		: FTrail(InOwner)
		, TrajectoryCache(MakeUnique<FAnimBoneTrajectoryCache>(InBoneName, InAnimTrajectoryCache))
		, CachedEffectiveRange(TRange<double>::Empty())
		, bIsRootBone(bInIsRootBone)
	{
		DrawInfo = MakeUnique<FTrajectoryDrawInfo>(InColor, TrajectoryCache.Get());
	}

	// FTrail interface
	virtual ETrailCacheState UpdateTrail(const FSceneContext& InSceneContext) override;
	virtual FTrajectoryCache* GetTrajectoryTransforms() override { return TrajectoryCache.Get(); }
	virtual TRange<double> GetEffectiveRange() const override { return CachedEffectiveRange; }
	// End FTrail interface

private:
	TUniquePtr<FAnimBoneTrajectoryCache> TrajectoryCache;

	TRange<double> CachedEffectiveRange;
	bool bIsRootBone;
};

} // namespace MovieScene
} // namespace UE
