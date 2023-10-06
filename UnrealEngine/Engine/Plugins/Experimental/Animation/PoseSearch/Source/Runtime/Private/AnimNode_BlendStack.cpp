// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_BlendStack.h"
#include "Algo/MaxElement.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimMontage.h"
#include "PoseSearch/PoseSearchDefines.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_BlendStack)

#if ENABLE_ANIM_DEBUG
TAutoConsoleVariable<int32> CVarAnimBlendStackEnable(TEXT("a.AnimNode.BlendStack.Enable"), 1, TEXT("Enable / Disable Blend Stack"));
TAutoConsoleVariable<int32> CVarAnimBlendStackPruningEnable(TEXT("a.AnimNode.BlendStack.Pruning.Enable"), 1, TEXT("Enable / Disable Blend Stack Pruning"));
#endif

/////////////////////////////////////////////////////
// FPoseSearchAnimPlayer
void FPoseSearchAnimPlayer::Initialize(UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop, bool bMirrored, UMirrorDataTable* MirrorDataTable, float BlendTime, float RootBoneBlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption InBlendOption, FVector BlendParameters, float PlayRate)
{
	check(AnimationAsset);

	if (bMirrored && !MirrorDataTable)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FPoseSearchAnimPlayer failed to Initialize for %s. Mirroring will not work becasue MirrorDataTable is missing"), *GetNameSafe(AnimationAsset));
	}

	const FReferenceSkeleton& RefSkeleton = AnimationAsset->GetSkeleton()->GetReferenceSkeleton();
	const bool bApplyDifferentRootBoneBlendTime = RootBoneBlendTime >= 0.f && !FMath::IsNearlyEqual(RootBoneBlendTime, BlendTime);
	const int32 NumSkeletonBones = RefSkeleton.GetNum();
	if (NumSkeletonBones <= 0)
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FPoseSearchAnimPlayer failed to Initialize for %s. Skeleton has no bones?!"), *GetNameSafe(AnimationAsset));
	}
	else if (BlendTime > UE_KINDA_SMALL_NUMBER)
	{
		// handling BlendTime > 0 and RootBoneBlendTime >= 0
		if (BlendProfile != nullptr)
		{
			check(BlendProfile->OwningSkeleton && NumSkeletonBones == BlendProfile->OwningSkeleton->GetReferenceSkeleton().GetNum());

			TotalBlendInTimePerBone.Init(BlendTime, NumSkeletonBones);

			BlendProfile->FillSkeletonBoneDurationsArray(TotalBlendInTimePerBone, BlendTime);

			if (bApplyDifferentRootBoneBlendTime)
			{
				TotalBlendInTimePerBone[RootBoneIndexType] *= RootBoneBlendTime / BlendTime;
			}

			BlendTime = *Algo::MaxElement(TotalBlendInTimePerBone);
		}
		else if (bApplyDifferentRootBoneBlendTime)
		{
			TotalBlendInTimePerBone.Init(BlendTime, NumSkeletonBones);
			TotalBlendInTimePerBone[RootBoneIndexType] *= RootBoneBlendTime / BlendTime;
			BlendTime = FMath::Max(BlendTime, RootBoneBlendTime);
		}
	}
	else if (bApplyDifferentRootBoneBlendTime)
	{
		// handling BlendTime ~= 0 and RootBoneBlendTime >= 0
		TotalBlendInTimePerBone.Init(BlendTime, NumSkeletonBones);
		TotalBlendInTimePerBone[RootBoneIndexType] = RootBoneBlendTime;
		BlendTime = FMath::Max(BlendTime, RootBoneBlendTime);
	}

	BlendOption = InBlendOption;

	TotalBlendInTime = BlendTime;
	CurrentBlendInTime = 0.f;

	MirrorNode.SetMirrorDataTable(MirrorDataTable);
	MirrorNode.SetMirror(bMirrored);
	
	if (Cast<UAnimMontage>(AnimationAsset))
	{
		UE_LOG(LogPoseSearch, Error, TEXT("FPoseSearchAnimPlayer unsupported AnimationAsset %s"), *GetNameSafe(AnimationAsset));
	}
	else if (UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationAsset))
	{
		SequencePlayerNode.SetAccumulatedTime(AccumulatedTime);
		SequencePlayerNode.SetSequence(SequenceBase);
		SequencePlayerNode.SetLoopAnimation(bLoop);
		SequencePlayerNode.SetPlayRate(PlayRate);
	}
	else if (UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset))
	{
		// making sure AccumulatedTime is in normalized space
		check(AccumulatedTime >= 0.f && AccumulatedTime <= 1.f);

		BlendSpacePlayerNode.SetResetPlayTimeWhenBlendSpaceChanges(false /*!bReset*/);
		BlendSpacePlayerNode.SetAccumulatedTime(AccumulatedTime);
		BlendSpacePlayerNode.SetBlendSpace(BlendSpace);
		BlendSpacePlayerNode.SetLoop(bLoop);
		BlendSpacePlayerNode.SetPlayRate(PlayRate);
		BlendSpacePlayerNode.SetPosition(BlendParameters);
	}
	else
	{
		checkNoEntry();
	}

	UpdateSourceLinkNode();
}

void FPoseSearchAnimPlayer::UpdatePlayRate(float PlayRate)
{
	if (SequencePlayerNode.GetSequence())
	{
		SequencePlayerNode.SetPlayRate(PlayRate);
	}
	else if (BlendSpacePlayerNode.GetBlendSpace())
	{
		BlendSpacePlayerNode.SetPlayRate(PlayRate);
	}
}

void FPoseSearchAnimPlayer::StorePoseContext(const FPoseContext& PoseContext)
{
	SequencePlayerNode.SetSequence(nullptr);
	BlendSpacePlayerNode.SetBlendSpace(nullptr);
	MirrorNode.SetSourceLinkNode(nullptr);

	if (PoseContext.Pose.IsValid())
	{
		StoredPose.CopyBonesFrom(PoseContext.Pose);
	}

	StoredCurve.CopyFrom(PoseContext.Curve);
	StoredAttributes.CopyFrom(PoseContext.CustomAttributes);
}

void FPoseSearchAnimPlayer::RestorePoseContext(FPoseContext& PoseContext) const
{
	check(!SequencePlayerNode.GetSequence() && !BlendSpacePlayerNode.GetBlendSpace());

	if (StoredPose.IsValid() && PoseContext.Pose.GetNumBones() == StoredPose.GetNumBones())
	{
		PoseContext.Pose.CopyBonesFrom(StoredPose);
	}
	else
	{
		PoseContext.Pose.ResetToRefPose();
	}
	
	PoseContext.Curve.CopyFrom(StoredCurve);
	PoseContext.CustomAttributes.CopyFrom(StoredAttributes);
}


// @todo: maybe implement copy/move constructors and assignment operator do so (or use a list instead of an array)
// since we're making copies and moving this object in memory, we're using this method to set the MirrorNode SourceLinkNode when necessary
void FPoseSearchAnimPlayer::UpdateSourceLinkNode()
{
	if (SequencePlayerNode.GetSequence())
	{
		MirrorNode.SetSourceLinkNode(&SequencePlayerNode);
	}
	else if (BlendSpacePlayerNode.GetBlendSpace())
	{
		MirrorNode.SetSourceLinkNode(&BlendSpacePlayerNode);
	}
	else
	{
		MirrorNode.SetSourceLinkNode(nullptr);
	}
}

void FPoseSearchAnimPlayer::Evaluate_AnyThread(FPoseContext& Output)
{
	if (SequencePlayerNode.GetSequence() || BlendSpacePlayerNode.GetBlendSpace())
	{
		UpdateSourceLinkNode();
		MirrorNode.Evaluate_AnyThread(Output);
	}
	else
	{
		RestorePoseContext(Output);
	}
}

void FPoseSearchAnimPlayer::Update_AnyThread(const FAnimationUpdateContext& Context, float BlendWeight)
{
	const FAnimationUpdateContext AnimPlayerContext = Context.FractionalWeightAndRootMotion(BlendWeight, BlendWeight);

	UpdateSourceLinkNode();
	MirrorNode.Update_AnyThread(AnimPlayerContext);
	CurrentBlendInTime += AnimPlayerContext.GetDeltaTime();
}

float FPoseSearchAnimPlayer::GetAccumulatedTime() const
{
	if (SequencePlayerNode.GetSequence())
	{
		return SequencePlayerNode.GetAccumulatedTime();
	}
	
	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		// making sure BlendSpacePlayerNode.GetAccumulatedTime() is in normalized space
		check(BlendSpacePlayerNode.GetAccumulatedTime() >= 0.f && BlendSpacePlayerNode.GetAccumulatedTime() <= 1.f);
		return BlendSpacePlayerNode.GetAccumulatedTime();
	}

	return 0.f;
}

FVector FPoseSearchAnimPlayer::GetBlendParameters() const
{
	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		return BlendSpacePlayerNode.GetPosition();
	}

	return FVector::ZeroVector;
}

FString FPoseSearchAnimPlayer::GetAnimationName() const
{
	if (SequencePlayerNode.GetSequence())
	{
		check(SequencePlayerNode.GetSequence());
		return SequencePlayerNode.GetSequence()->GetName();
	}

	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		check(BlendSpacePlayerNode.GetBlendSpace());
		return BlendSpacePlayerNode.GetBlendSpace()->GetName();
	}

	return FString("StoredPose");
}

const UAnimationAsset* FPoseSearchAnimPlayer::GetAnimationAsset() const
{
	if (SequencePlayerNode.GetSequence())
	{
		return SequencePlayerNode.GetSequence();
	}

	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		return BlendSpacePlayerNode.GetBlendSpace();
	}

	return nullptr;
}

float FPoseSearchAnimPlayer::GetBlendInPercentage() const
{
	if (FMath::IsNearlyZero(TotalBlendInTime))
	{
		return 1.f;
	}

	return FMath::Clamp(CurrentBlendInTime / TotalBlendInTime, 0.f, 1.f);
}

bool FPoseSearchAnimPlayer::GetBlendInWeights(TArray<float>& Weights) const
{
	const int32 NumBones = TotalBlendInTimePerBone.Num();
	if (NumBones > 0)
	{
		Weights.SetNumUninitialized(NumBones);
		for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
		{
			const float TotalBlendInTimeBoneIdx = TotalBlendInTimePerBone[BoneIdx];
			if (FMath::IsNearlyZero(TotalBlendInTimeBoneIdx))
			{
				Weights[BoneIdx] = 1.f;
			}
			else
			{
				const float unclampedLinearWeight = CurrentBlendInTime / TotalBlendInTimeBoneIdx;
				Weights[BoneIdx] = FAlphaBlend::AlphaToBlendOption(unclampedLinearWeight, BlendOption);
			}
		}
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////
// FAnimNode_BlendStack_Standalone
void FAnimNode_BlendStack_Standalone::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BlendStack_Evaluate_AnyThread);

	Super::Evaluate_AnyThread(Output);

	const int32 BlendStackSize = AnimPlayers.Num();
	if (BlendStackSize <= 0)
	{
		Output.ResetToRefPose();
	}
	else if (BlendStackSize == 1)
	{
		AnimPlayers.First().Evaluate_AnyThread(Output);
	}
	else if (RequestedMaxActiveBlends <= 0 
#if ENABLE_ANIM_DEBUG
		|| !CVarAnimBlendStackEnable.GetValueOnAnyThread()
#endif // ENABLE_ANIM_DEBUG
		)
	{
		// Disable blend stack if requested (for testing / debugging) by removing all the AnimPlayers except the first
		while (AnimPlayers.Num() > 1)
		{
			AnimPlayers.PopLast();
		}
		AnimPlayers.First().Evaluate_AnyThread(Output);
	}
	else
	{
		// evaluating the last AnimPlayer into Output...
		AnimPlayers[BlendStackSize - 1].Evaluate_AnyThread(Output);
		 
		FPoseContext EvaluationPoseContext(Output);
		FPoseContext BlendedPoseContext(Output); // @todo: this should not be necessary (but FBaseBlendedCurve::InitFrom complains about "ensure(&InCurveToInitFrom != this)"): optimize it away!
		FAnimationPoseData BlendedAnimationPoseData(BlendedPoseContext);

		const USkeleton* SkeletonAsset = Output.AnimInstanceProxy->GetRequiredBones().GetSkeletonAsset();
		check(SkeletonAsset);

		const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
		const int32 NumSkeletonBones = RefSkeleton.GetNum();
		TArray<float> Weights;

		// evaluating from the second last to the first into EvaluationPoseContext and then blend it with the with the Output (initialized with the last AnimPlayer evaluation)
		for (int32 i = BlendStackSize - 2; i >= 0; --i)
		{
			AnimPlayers[i].Evaluate_AnyThread(EvaluationPoseContext);
			
			if (AnimPlayers[i].GetBlendInWeights(Weights))
			{
				// @todo: have BlendTwoPosesTogetherPerBone using a TArrayView for the Weights to avoid allocations
				FAnimationRuntime::BlendTwoPosesTogetherPerBone(FAnimationPoseData(Output), FAnimationPoseData(EvaluationPoseContext), Weights, BlendedAnimationPoseData);
			}
			else
			{
				const float Weight = 1.f - FAlphaBlend::AlphaToBlendOption(AnimPlayers[i].GetBlendInPercentage(), AnimPlayers[i].GetBlendOption());
				FAnimationRuntime::BlendTwoPosesTogether(FAnimationPoseData(Output), FAnimationPoseData(EvaluationPoseContext), Weight, BlendedAnimationPoseData);
			}
			Output = BlendedPoseContext; // @todo: this should not be necessary either: optimize it away!

			if (i >= RequestedMaxActiveBlends
#if ENABLE_ANIM_DEBUG
				&& CVarAnimBlendStackPruningEnable.GetValueOnAnyThread()
#endif // ENABLE_ANIM_DEBUG
				)
			{
				// too many AnimPlayers! we don't have enough available blends to hold them all, so we accumulate the blended poses into Output / BlendedPoseContext, until...
				AnimPlayers.PopLast();
				
				if (i == RequestedMaxActiveBlends)
				{
					check(AnimPlayers.Num() == RequestedMaxActiveBlends + 1);

					// we can store Output / BlendedPoseContext into the last AnimPlayer, that will hold a static pose, no longer an animation playing
					AnimPlayers[i].StorePoseContext(Output);
				}
			}
		}

		const int32 ActiveBlends = AnimPlayers.Num() - 1;
		if (ActiveBlends > RequestedMaxActiveBlends)
		{
			UE_LOG(LogPoseSearch, Display, TEXT("FAnimNode_BlendStack_Standalone NumBlends/MaxNumBlends %d / %d"), ActiveBlends, RequestedMaxActiveBlends);
		}
	}
}

void FAnimNode_BlendStack_Standalone::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BlendStack_UpdateAssetPlayer);

	Super::UpdateAssetPlayer(Context);

	// AnimPlayers[0] is the most newly inserted AnimPlayer, AnimPlayers[AnimPlayers.Num()-1] is the oldest, so to calculate the weights
	// we ask AnimPlayers[0] its BlendInPercentage and then distribute the left over (CurrentWeightMultiplier) to the rest of the AnimPlayers
	// AnimPlayers[AnimPlayerIndex].GetBlendWeight() will now store the weighted contribution of AnimPlayers[AnimPlayerIndex] to be able to calculate root motion from animation
	float CurrentWeightMultiplier = 1.f;
	const int32 BlendStackSize = AnimPlayers.Num();
	int32 AnimPlayerIndex = 0;
	for (; AnimPlayerIndex < BlendStackSize; ++AnimPlayerIndex)
	{
		FPoseSearchAnimPlayer& AnimPlayer = AnimPlayers[AnimPlayerIndex];
		const bool bIsLastAnimPlayers = AnimPlayerIndex == BlendStackSize - 1;
		const float BlendInPercentage = bIsLastAnimPlayers ? 1.f : AnimPlayer.GetBlendInPercentage();
		const float AnimPlayerBlendWeight = CurrentWeightMultiplier * BlendInPercentage;

		// don't break for AnimPlayerIndex == 0 since FAnimNode_BlendStack_Standalone::BlendTo initialize the AnimPlayer with a weight of zero
		if (AnimPlayerIndex > 0 && AnimPlayerBlendWeight < UE_KINDA_SMALL_NUMBER)
		{
			break;
		}

		AnimPlayer.Update_AnyThread(Context, AnimPlayerBlendWeight);

		CurrentWeightMultiplier *= (1.f - BlendInPercentage);
	}

	// AnimPlayers[AnimPlayerIndex] is the first FPoseSearchAnimPlayer with a weight contribution of zero, so we can discard it and all the successive AnimPlayers as well
	const int32 WantedAnimPlayersNum = FMath::Max(1, AnimPlayerIndex); // we save at least one FPoseSearchAnimPlayer
	while (AnimPlayers.Num() > WantedAnimPlayersNum)
	{
		AnimPlayers.PopLast();
	}
}

float FAnimNode_BlendStack_Standalone::GetAccumulatedTime() const
{
	return AnimPlayers.IsEmpty() ? 0.f : AnimPlayers.First().GetAccumulatedTime();
}

void FAnimNode_BlendStack_Standalone::BlendTo(UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop, bool bMirrored, UMirrorDataTable* MirrorDataTable, int32 MaxActiveBlends, float BlendTime, float RootBoneBlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption BlendOption, FVector BlendParameters, float PlayRate)
{
	RequestedMaxActiveBlends = MaxActiveBlends;
	AnimPlayers.PushFirst(FPoseSearchAnimPlayer());
	AnimPlayers.First().Initialize(AnimationAsset, AccumulatedTime, bLoop, bMirrored, MirrorDataTable, BlendTime, RootBoneBlendTime, BlendProfile, BlendOption, BlendParameters, PlayRate);
}

void FAnimNode_BlendStack_Standalone::UpdatePlayRate(float PlayRate)
{
	if (!AnimPlayers.IsEmpty())
	{
		AnimPlayers.First().UpdatePlayRate(PlayRate);
	}
}

void FAnimNode_BlendStack_Standalone::GatherDebugData(FNodeDebugData& DebugData)
{
#if ENABLE_ANIM_DEBUG
	DebugData.AddDebugItem(FString::Printf(TEXT("%s"), *DebugData.GetNodeName(this)));
	for (int32 i = 0; i < AnimPlayers.Num(); ++i)
	{
		const FPoseSearchAnimPlayer& AnimPlayer = AnimPlayers[i];
		DebugData.AddDebugItem(FString::Printf(TEXT("%d) t:%.2f/%.2f m:%d %s"),
				i, AnimPlayer.GetCurrentBlendInTime(), AnimPlayer.GetTotalBlendInTime(),
				AnimPlayer.GetMirror() ? 1 : 0, *AnimPlayer.GetAnimationName()));
	}
#endif // ENABLE_ANIM_DEBUG

	// propagating GatherDebugData to the AnimPlayers
	for (FPoseSearchAnimPlayer& AnimPlayer : AnimPlayers)
	{
		AnimPlayer.GetMirrorNode().GatherDebugData(DebugData);
	}
}

/////////////////////////////////////////////////////
// FAnimNode_BlendStack

void FAnimNode_BlendStack::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	GetEvaluateGraphExposedInputs().Execute(Context);

	if (AnimationAsset)
	{
		bool bExecuteBlendTo = false;
		if (AnimPlayers.IsEmpty())
		{
			bExecuteBlendTo = true;
		}
		else
		{
			const FPoseSearchAnimPlayer& MainAnimPlayer = AnimPlayers.First();
			const UAnimationAsset* PlayingAnimationAsset = MainAnimPlayer.GetAnimationAsset();
			check(PlayingAnimationAsset);

			if (AnimationAsset != PlayingAnimationAsset)
			{
				bExecuteBlendTo = true;
			}
			else if (bMirrored != MainAnimPlayer.GetMirror())
			{
				bExecuteBlendTo = true;
			}
			else if (BlendParameters != MainAnimPlayer.GetBlendParameters())
			{
				bExecuteBlendTo = true;
			}
			else if (MaxAnimationDeltaTime >= 0.f && FMath::Abs(AnimationTime - MainAnimPlayer.GetAccumulatedTime()) > MaxAnimationDeltaTime)
			{
				bExecuteBlendTo = true;
			}
		}

		if (bExecuteBlendTo)
		{
			BlendTo(AnimationAsset, AnimationTime, bLoop, bMirrored, MirrorDataTable.Get(), MaxActiveBlends, BlendTime, RootBoneBlendTime, BlendProfile, BlendOption, BlendParameters, WantedPlayRate);
		}
	}
	
	UpdatePlayRate(WantedPlayRate);

	Super::UpdateAssetPlayer(Context);
}
