// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeMessages.h"
#include "Animation/AnimStats.h"
#include "Engine/SkinnedAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_PoseSearchHistoryCollector)

#define LOCTEXT_NAMESPACE "AnimNode_PoseSearchHistoryCollector"

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
TAutoConsoleVariable<bool> CVarAnimPoseHistoryDebugDraw(TEXT("a.AnimNode.PoseHistory.DebugDraw"), false, TEXT("Enable / Disable Pose History DebugDraw"));
#endif

namespace UE::PoseSearch::Private
{

class FPoseHistoryProvider : public IPoseHistoryProvider
{
public:
	FPoseHistoryProvider(const IPoseHistory& InPoseHistory)
		: PoseHistory(InPoseHistory)
	{
	}

	// IPoseHistoryProvider interface
	virtual const IPoseHistory& GetPoseHistory() const override
	{
		return PoseHistory;
	}

private:
	const IPoseHistory& PoseHistory;
};

} // namespace UE::PoseSearch::Private


/////////////////////////////////////////////////////
// FAnimNode_PoseSearchHistoryCollector_Base

void FAnimNode_PoseSearchHistoryCollector_Base::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);
	check(Context.AnimInstanceProxy);

	Super::CacheBones_AnyThread(Context);

	TArray<FBoneIndexType> RequiredBones;
	if (!CollectedBones.IsEmpty())
	{
		if (const USkeletalMeshComponent* SkeletalMeshComponent = Context.AnimInstanceProxy->GetSkelMeshComponent())
		{
			if (const USkinnedAsset* SkinnedAsset = SkeletalMeshComponent->GetSkinnedAsset())
			{
				if (const USkeleton* Skeleton = SkinnedAsset->GetSkeleton())
				{
					RequiredBones.Reserve(CollectedBones.Num());
					for (FBoneReference& BoneReference : CollectedBones)
					{
						if (BoneReference.Initialize(Skeleton))
						{
							RequiredBones.AddUnique(BoneReference.BoneIndex);
						}
					}
				}
			}
		}
	}

	PoseHistory.Init(PoseCount, PoseDuration, RequiredBones);
}

/////////////////////////////////////////////////////
// FAnimNode_PoseSearchHistoryCollector

void FAnimNode_PoseSearchHistoryCollector::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);
	Super::Initialize_AnyThread(Context);
	Source.Initialize(Context);
}

void FAnimNode_PoseSearchHistoryCollector::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	Super::CacheBones_AnyThread(Context);
	Source.CacheBones(Context);
}

void FAnimNode_PoseSearchHistoryCollector::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(PoseSearchHistoryCollector, !IsInGameThread());

	check(Output.AnimInstanceProxy);

	Super::Evaluate_AnyThread(Output);
	Source.Evaluate(Output);

	FCSPose<FCompactPose> ComponentSpacePose;
	ComponentSpacePose.InitPose(Output.Pose);
	PoseHistory.Update(Output.AnimInstanceProxy->GetDeltaSeconds(), ComponentSpacePose, Output.AnimInstanceProxy->GetComponentTransform());

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	if (CVarAnimPoseHistoryDebugDraw.GetValueOnAnyThread())
	{
		PoseHistory.DebugDraw(*Output.AnimInstanceProxy);
	}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
}

void FAnimNode_PoseSearchHistoryCollector::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);
	Super::Update_AnyThread(Context);
	UE::Anim::TScopedGraphMessage<UE::PoseSearch::Private::FPoseHistoryProvider> ScopedMessage(Context, PoseHistory);
	Source.Update(Context);
}

void FAnimNode_PoseSearchHistoryCollector::GatherDebugData(FNodeDebugData& DebugData)
{
	Super::GatherDebugData(DebugData);
	Source.GatherDebugData(DebugData);
}


/////////////////////////////////////////////////////
// FAnimNode_PoseSearchComponentSpaceHistoryCollector

void FAnimNode_PoseSearchComponentSpaceHistoryCollector::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);
	Super::Initialize_AnyThread(Context);
	Source.Initialize(Context);
}

void FAnimNode_PoseSearchComponentSpaceHistoryCollector::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	Super::CacheBones_AnyThread(Context);
	Source.CacheBones(Context);
}

void FAnimNode_PoseSearchComponentSpaceHistoryCollector::EvaluateComponentSpace_AnyThread(FComponentSpacePoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(EvaluateComponentSpace_AnyThread);
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(PoseSearchComponentSpaceHistoryCollector, !IsInGameThread());

	check(Output.AnimInstanceProxy);

	Super::EvaluateComponentSpace_AnyThread(Output);
	Source.EvaluateComponentSpace(Output);

	PoseHistory.Update(Output.AnimInstanceProxy->GetDeltaSeconds(), Output.Pose, Output.AnimInstanceProxy->GetComponentTransform());

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	if (CVarAnimPoseHistoryDebugDraw.GetValueOnAnyThread())
	{
		PoseHistory.DebugDraw(*Output.AnimInstanceProxy);
	}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
}

void FAnimNode_PoseSearchComponentSpaceHistoryCollector::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Update_AnyThread);
	Super::Update_AnyThread(Context);
	UE::Anim::TScopedGraphMessage<UE::PoseSearch::Private::FPoseHistoryProvider> ScopedMessage(Context, PoseHistory);
	Source.Update(Context);
}

void FAnimNode_PoseSearchComponentSpaceHistoryCollector::GatherDebugData(FNodeDebugData& DebugData)
{
	Super::GatherDebugData(DebugData);
	Source.GatherDebugData(DebugData);
}

#undef LOCTEXT_NAMESPACE
