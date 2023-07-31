// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_BlendStack.h"

#include "Algo/MaxElement.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"

#define LOCTEXT_NAMESPACE "AnimNode_BlendStack"

#if ENABLE_ANIM_DEBUG
TAutoConsoleVariable<int32> CVarAnimBlendStackEnable(TEXT("a.AnimNode.BlendStack.Enable"), 1, TEXT("Enable / Disable Blend Stack"));
TAutoConsoleVariable<int32> CVarAnimBlendStackPruningEnable(TEXT("a.AnimNode.BlendStack.Pruning.Enable"), 1, TEXT("Enable / Disable Blend Stack Pruning"));
#endif

/////////////////////////////////////////////////////
// FPoseSearchAnimPlayer
void FPoseSearchAnimPlayer::Initialize(ESearchIndexAssetType InAssetType, UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop, bool bMirrored, UMirrorDataTable* MirrorDataTable, float BlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption InBlendOption, FVector BlendParameters)
{
	check(AnimationAsset);

	if (bMirrored && !MirrorDataTable)
	{
		UE_LOG(
			LogPoseSearch,
			Error,
			TEXT("FPoseSearchAnimPlayer failed to Initialize for %s. Mirroring will not work becasue MirrorDataTable is missing"),
			*GetNameSafe(AnimationAsset));
	}

	if (BlendProfile != nullptr)
	{
		const USkeleton* SkeletonAsset = BlendProfile->OwningSkeleton;
		check(SkeletonAsset);

		const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
		const int32 NumSkeletonBones = RefSkeleton.GetNum();
		TotalBlendInTimePerBone.Init(BlendTime, NumSkeletonBones);

		BlendProfile->FillSkeletonBoneDurationsArray(TotalBlendInTimePerBone, BlendTime);
		BlendTime = *Algo::MaxElement(TotalBlendInTimePerBone);
	}
	BlendOption = InBlendOption;

	AssetType = InAssetType;
	TotalBlendInTime = BlendTime;
	CurrentBlendInTime = 0.f;

	MirrorNode.SetMirrorDataTable(MirrorDataTable);
	MirrorNode.SetMirror(bMirrored);
	
	if (AssetType == ESearchIndexAssetType::Sequence)
	{
		UAnimSequenceBase* Sequence = Cast<UAnimSequenceBase>(AnimationAsset);
		check(Sequence);

		SequencePlayerNode.SetAccumulatedTime(AccumulatedTime);
		SequencePlayerNode.SetSequence(Sequence);
		SequencePlayerNode.SetLoopAnimation(bLoop);
		SequencePlayerNode.SetPlayRate(1.0f);
	}
	else if (AssetType == ESearchIndexAssetType::BlendSpace)
	{
		UBlendSpace* BlendSpace = Cast<UBlendSpace>(AnimationAsset);
		check(BlendSpace);
		BlendSpacePlayerNode.SetResetPlayTimeWhenBlendSpaceChanges(false /*!bReset*/);
		BlendSpacePlayerNode.SetAccumulatedTime(AccumulatedTime);
		BlendSpacePlayerNode.SetBlendSpace(BlendSpace);
		BlendSpacePlayerNode.SetLoop(bLoop);
		BlendSpacePlayerNode.SetPlayRate(1.0f);
		BlendSpacePlayerNode.SetPosition(BlendParameters);
	}
	else 
	{
		checkNoEntry();
	}
	UpdateSourceLinkNode();
}

void FPoseSearchAnimPlayer::StorePoseContext(const FPoseContext& PoseContext)
{
	AssetType = ESearchIndexAssetType::Invalid;
	UpdateSourceLinkNode();

	const FBoneContainer& BoneContainer = PoseContext.Pose.GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	check(SkeletonAsset);

	const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
	const int32 NumSkeletonBones = RefSkeleton.GetNum();

	StoredPose.SetNum(NumSkeletonBones);
	for (FSkeletonPoseBoneIndex SkeletonBoneIdx(0); SkeletonBoneIdx != NumSkeletonBones; ++SkeletonBoneIdx)
	{
		FCompactPoseBoneIndex CompactBoneIdx = BoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonBoneIdx);
		StoredPose[SkeletonBoneIdx.GetInt()] = CompactBoneIdx.IsValid() ? PoseContext.Pose[CompactBoneIdx] : RefSkeleton.GetRefBonePose()[SkeletonBoneIdx.GetInt()];
	}

	// @todo: perhaps copy PoseContext.Curve and PoseContext.CustomAttributes?
}

void FPoseSearchAnimPlayer::RestorePoseContext(FPoseContext& PoseContext) const
{
	check(AssetType == ESearchIndexAssetType::Invalid);

	const FBoneContainer& BoneContainer = PoseContext.Pose.GetBoneContainer();
	const USkeleton* SkeletonAsset = BoneContainer.GetSkeletonAsset();
	check(SkeletonAsset);

	const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
	const int32 NumSkeletonBones = RefSkeleton.GetNum();

	for (const FBoneIndexType BoneIdx : BoneContainer.GetBoneIndicesArray())
	{
		const FCompactPoseBoneIndex CompactBoneIdx(BoneIdx);
		const FSkeletonPoseBoneIndex SkeletonBoneIdx = BoneContainer.GetSkeletonPoseIndexFromCompactPoseIndex(CompactBoneIdx);
		if (SkeletonBoneIdx.IsValid() && BoneIdx < StoredPose.Num())
		{
			PoseContext.Pose[CompactBoneIdx] = StoredPose[SkeletonBoneIdx.GetInt()];
		}
	}

	// @todo: perhaps copy PoseContext.Curve and PoseContext.CustomAttributes?
}


// @todo: maybe implement copy/move constructors and assignement operator do so (or use a list instead of an array)
// since we're making copies and moving this object in memory, we're using this method to set the MirrorNode SourceLinkNode when necessary
void FPoseSearchAnimPlayer::UpdateSourceLinkNode()
{
	if (AssetType == ESearchIndexAssetType::Sequence)
	{
		MirrorNode.SetSourceLinkNode(&SequencePlayerNode);
	}
	else if (AssetType == ESearchIndexAssetType::BlendSpace)
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
	if (AssetType != ESearchIndexAssetType::Invalid)
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
	if (AssetType == ESearchIndexAssetType::Sequence)
	{
		return SequencePlayerNode.GetAccumulatedTime();
	}
	
	if (AssetType == ESearchIndexAssetType::BlendSpace)
	{
		return BlendSpacePlayerNode.GetAccumulatedTime();
	}
	
	return 0.f;
}

FString FPoseSearchAnimPlayer::GetAnimName() const
{
	if (AssetType == ESearchIndexAssetType::Sequence)
	{
		check(SequencePlayerNode.GetSequence());
		return SequencePlayerNode.GetSequence()->GetName();
	}

	if (AssetType == ESearchIndexAssetType::BlendSpace)
	{
		check(BlendSpacePlayerNode.GetBlendSpace());
		return BlendSpacePlayerNode.GetBlendSpace()->GetName();
	}

	return FString("StoredPose");
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
// FAnimNode_BlendStack
void FAnimNode_BlendStack::Evaluate_AnyThread(FPoseContext& Output)
{
	Super::Evaluate_AnyThread(Output);

	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);

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
				// too many AnimPlayers! we don't have enought available blends to hold them all, so we accumulate the blended poses into Output / BlendedPoseContext, until...
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
			UE_LOG(LogPoseSearch, Display, TEXT("FAnimNode_BlendStack NumBlends/MaxNumBlends %d / %d"), ActiveBlends, RequestedMaxActiveBlends);
		}
	}
}

void FAnimNode_BlendStack::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
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

		// don't break for AnimPlayerIndex == 0 since FAnimNode_BlendStack::BlendTo initialize the AnimPlayer with a weight of zero
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

float FAnimNode_BlendStack::GetAccumulatedTime() const
{
	return AnimPlayers.IsEmpty() ? 0.f : AnimPlayers.First().GetAccumulatedTime();
}

void FAnimNode_BlendStack::BlendTo(ESearchIndexAssetType AssetType, UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop, bool bMirrored, UMirrorDataTable* MirrorDataTable, int32 MaxActiveBlends, float BlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption BlendOption, FVector BlendParameters)
{
	RequestedMaxActiveBlends = MaxActiveBlends;
	AnimPlayers.PushFirst(FPoseSearchAnimPlayer());
	AnimPlayers.First().Initialize(AssetType, AnimationAsset, AccumulatedTime, bLoop, bMirrored, MirrorDataTable, BlendTime, BlendProfile, BlendOption, BlendParameters);
}

void FAnimNode_BlendStack::GatherDebugData(FNodeDebugData& DebugData)
{
#if ENABLE_ANIM_DEBUG
	DebugData.AddDebugItem(FString::Printf(TEXT("%s"), *DebugData.GetNodeName(this)));
	for (int32 i = 0; i < AnimPlayers.Num(); ++i)
	{
		const FPoseSearchAnimPlayer& AnimPlayer = AnimPlayers[i];
		DebugData.AddDebugItem(FString::Printf(TEXT("%d) t:%.2f/%.2f m:%d %s"),
				i, AnimPlayer.GetCurrentBlendInTime(), AnimPlayer.GetTotalBlendInTime(),
				AnimPlayer.GetMirror() ? 1 : 0, *AnimPlayer.GetAnimName()));
	}
#endif // ENABLE_ANIM_DEBUG

	// propagating GatherDebugData to the AnimPlayers
	for (FPoseSearchAnimPlayer& AnimPlayer : AnimPlayers)
	{
		AnimPlayer.GetMirrorNode().GatherDebugData(DebugData);
	}
}

#undef LOCTEXT_NAMESPACE