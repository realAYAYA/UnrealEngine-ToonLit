// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimNodes/AnimNode_PoseBlendNode.h"
#include "AnimationRuntime.h"
#include "Animation/AnimInstanceProxy.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_PoseBlendNode)

/////////////////////////////////////////////////////
// FAnimPoseByNameNode

FAnimNode_PoseBlendNode::FAnimNode_PoseBlendNode()
	: CustomCurve(nullptr)
{
	BlendOption = EAlphaBlendOption::Linear;
}

void FAnimNode_PoseBlendNode::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread)
	FAnimNode_PoseHandler::Initialize_AnyThread(Context);

	SourcePose.Initialize(Context);
}

void FAnimNode_PoseBlendNode::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(CacheBones_AnyThread)
	FAnimNode_PoseHandler::CacheBones_AnyThread(Context);
	SourcePose.CacheBones(Context);
}

void FAnimNode_PoseBlendNode::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	FAnimNode_PoseHandler::UpdateAssetPlayer(Context);
	SourcePose.Update(Context);
}

void FAnimNode_PoseBlendNode::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread)
	ANIM_MT_SCOPE_CYCLE_COUNTER(PoseBlendNodeEvaluate, !IsInGameThread());

	FPoseContext SourceData(Output);
	SourcePose.Evaluate(SourceData);

	bool bValidPose = false;

	if (CurrentPoseAsset.IsValid() && (PoseExtractContext.PoseCurves.Num() > 0) && (Output.AnimInstanceProxy->IsSkeletonCompatible(CurrentPoseAsset->GetSkeleton())))
	{
		const UPoseAsset* CachedPoseAsset = CurrentPoseAsset.Get();
		FPoseContext CurrentPose(Output);
		// only give pose curve, we don't set any more curve here
		for (int32 PoseIdx = 0; PoseIdx < PoseExtractContext.PoseCurves.Num(); ++PoseIdx)
		{
			FPoseCurve& PoseCurve = PoseExtractContext.PoseCurves[PoseIdx];
			// Get value of input curve
			float InputValue = SourceData.Curve.Get(PoseCurve.UID);
			// Remap using chosen BlendOption
			float RemappedValue = FAlphaBlend::AlphaToBlendOption(InputValue, BlendOption, CustomCurve);

			PoseCurve.Value = RemappedValue;
		}


		FAnimationPoseData CurrentAnimationPoseData(CurrentPose);
		if (CachedPoseAsset->GetAnimationPose(CurrentAnimationPoseData, PoseExtractContext))
		{
			// once we get it, we have to blend by weight
			if (CachedPoseAsset->IsValidAdditive())
			{
				Output = SourceData;

				FAnimationPoseData BaseAnimationPoseData(Output);
				FAnimationRuntime::AccumulateAdditivePose(BaseAnimationPoseData, CurrentAnimationPoseData, 1.f, EAdditiveAnimationType::AAT_LocalSpaceBase);
			}
			else
			{

				FAnimationPoseData OutputAnimationPoseData(Output);
				const FAnimationPoseData SourceAnimationPoseData(SourceData);

				FAnimationRuntime::BlendTwoPosesTogetherPerBone(SourceAnimationPoseData, CurrentAnimationPoseData, BoneBlendWeights, OutputAnimationPoseData);
			}

			bValidPose = true;
		}
	}

	// If we didn't create a valid pose, just copy SourcePose to output (pass through)
	if(!bValidPose)
	{
		Output = SourceData;
	}
}

void FAnimNode_PoseBlendNode::GatherDebugData(FNodeDebugData& DebugData)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(GatherDebugData)
	FAnimNode_PoseHandler::GatherDebugData(DebugData);
	SourcePose.GatherDebugData(DebugData.BranchFlow(1.f));
}


