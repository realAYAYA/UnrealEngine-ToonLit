// Copyright Epic Games, Inc. All Rights Reserved.

#include "BlendStack/AnimNode_BlendStack.h"
#include "Algo/MaxElement.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "BlendStack/AnimNode_BlendStackInput.h"
#include "BlendStackAnimEventsFilterScope.h"
#include "Animation/AnimInertializationSyncScope.h"
#include "BlendStack/BlendStackDefines.h"
#include "VisualLogger/VisualLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_BlendStack)

#if ENABLE_ANIM_DEBUG
static TAutoConsoleVariable<bool> CVarAnimBlendStackEnable(TEXT("a.AnimNode.BlendStack.Enable"), true, TEXT("Enable / Disable Blend Stack"));
#endif

#define LOCTEXT_NAMESPACE "AnimNode_BlendStack"

static constexpr FBoneIndexType RootBoneIndexType = 0;

/////////////////////////////////////////////////////
// FBlendStackAnimPlayer
void FBlendStackAnimPlayer::Initialize(const FAnimationInitializeContext& Context, UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop,
	bool bMirrored, UMirrorDataTable* MirrorDataTable, float BlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption InBlendOption,
	const FVector& BlendParameters, float PlayRate, float ActivationDelay, int32 InPoseLinkIdx, FName GroupName, EAnimGroupRole::Type GroupRole, EAnimSyncMethod GroupMethod)
{
	if (bMirrored && !MirrorDataTable)
	{
		UE_LOG(LogBlendStack, Error, TEXT("FBlendStackAnimPlayer failed to Initialize for %s. Mirroring will not work becasue MirrorDataTable is missing"), *GetNameSafe(AnimationAsset));
	}
	
	check(Context.AnimInstanceProxy);
	USkeleton* Skeleton = Context.AnimInstanceProxy->GetSkeleton();
	check(Skeleton);

	const FReferenceSkeleton& RefSkeleton = Skeleton->GetReferenceSkeleton();
	const int32 NumSkeletonBones = RefSkeleton.GetNum();
	if (NumSkeletonBones <= 0)
	{
		UE_LOG(LogBlendStack, Error, TEXT("FBlendStackAnimPlayer failed to Initialize for %s. Skeleton has no bones?!"), *GetNameSafe(AnimationAsset));
	}
	else if (BlendTime > UE_KINDA_SMALL_NUMBER)
	{
		// handling BlendTime > 0 and RootBoneBlendTime >= 0
		if (BlendProfile != nullptr)
		{
			TotalBlendInTimePerBone.Init(BlendTime, NumSkeletonBones);
			BlendProfile->FillSkeletonBoneDurationsArray(TotalBlendInTimePerBone, BlendTime, Skeleton);
			BlendTime = *Algo::MaxElement(TotalBlendInTimePerBone);
		}
	}

	BlendOption = InBlendOption;

	TotalBlendInTime = BlendTime;
	CurrentBlendInTime = 0.f;
	TimeToActivation = ActivationDelay;

	MirrorNode.SetMirrorDataTable(MirrorDataTable);
	MirrorNode.SetMirror(bMirrored);
	
	bool bUnsupportedAnimAsset = false;
	if (Cast<UAnimMontage>(AnimationAsset))
	{
		bUnsupportedAnimAsset = true;
	}
	else if (UAnimSequenceBase* SequenceBase = Cast<UAnimSequenceBase>(AnimationAsset))
	{
		SequencePlayerNode.SetAccumulatedTime(AccumulatedTime);
		SequencePlayerNode.SetSequence(SequenceBase);
		SequencePlayerNode.SetLoopAnimation(bLoop);
		SequencePlayerNode.SetPlayRate(PlayRate);
		SequencePlayerNode.SetGroupMethod(GroupMethod);
		SequencePlayerNode.SetGroupName(GroupName);
		SequencePlayerNode.SetGroupRole(GroupRole);
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
		BlendSpacePlayerNode.SetGroupMethod(GroupMethod);
		BlendSpacePlayerNode.SetGroupName(GroupName);
		BlendSpacePlayerNode.SetGroupRole(GroupRole);
	}
	else if (AnimationAsset)
	{
		bUnsupportedAnimAsset = true;
	}

	if (bUnsupportedAnimAsset)
	{
		UE_LOG(LogBlendStack, Error, TEXT("FBlendStackAnimPlayer unsupported AnimationAsset %s"), *GetNameSafe(AnimationAsset));
	}

	UpdateSourceLinkNode();
	PoseLinkIndex = InPoseLinkIdx;

	OverrideCurve.Empty();
}

void FBlendStackAnimPlayer::UpdatePlayRate(float PlayRate)
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

void FBlendStackAnimPlayer::StorePoseContext(const FPoseContext& PoseContext)
{
	SequencePlayerNode.SetSequence(nullptr);
	BlendSpacePlayerNode.SetBlendSpace(nullptr);
	MirrorNode.SetSourceLinkNode(nullptr);

	if (PoseContext.Pose.IsValid())
	{
		StoredBones = PoseContext.Pose.GetBones();
		StoredBoneContainer = PoseContext.Pose.GetBoneContainer();
	}

	StoredCurve.CopyFrom(PoseContext.Curve);
	StoredAttributes.CopyFrom(PoseContext.CustomAttributes);
}

bool FBlendStackAnimPlayer::HasValidPoseContext() const
{
	return !StoredBones.IsEmpty() && StoredBoneContainer.IsValid();
}

void FBlendStackAnimPlayer::MovePoseContextTo(FBlendStackAnimPlayer& Other)
{
	// moving the allocated memory to Other
	Other.StoredBones = MoveTemp(StoredBones);
	Other.StoredCurve = MoveTemp(StoredCurve);
	Other.StoredAttributes = MoveTemp(StoredAttributes);
	Other.StoredBoneContainer = MoveTemp(StoredBoneContainer);
	
	// making sure Other pose context is invalid
	Other.StoredBones.Reset();
}

void FBlendStackAnimPlayer::RestorePoseContext(FPoseContext& PoseContext) const
{
	check(!SequencePlayerNode.GetSequence() && !BlendSpacePlayerNode.GetBlendSpace());

	if (StoredBoneContainer.IsValid())
	{
		// Serial number mismatch means a potential bone LOD mismatch, even if we have the same number of bones.
		// Remap the pose manually in those cases.
		if (PoseContext.Pose.GetBoneContainer().GetSerialNumber() == StoredBoneContainer.GetSerialNumber())
		{
			if (StoredBones.IsEmpty())
			{
				PoseContext.Pose.ResetToRefPose();
			}
			else
			{
				check(PoseContext.Pose.GetNumBones() == StoredBones.Num());
				FMemory::Memcpy(PoseContext.Pose.GetMutableBones().GetData(), StoredBones.GetData(), sizeof(FTransform) * PoseContext.Pose.GetNumBones());
			}
		}
		else
		{
			const FBoneContainer CurrentBoneContainer = PoseContext.Pose.GetBoneContainer();
			for (FCompactPoseBoneIndex CompactPoseIndex : PoseContext.Pose.ForEachBoneIndex())
			{
				// Map the current compact pose index to skeleton index, and map this back to the stored compact pose index.
				const FSkeletonPoseBoneIndex SkeletonPoseIndex =  CurrentBoneContainer.GetSkeletonPoseIndexFromCompactPoseIndex(CompactPoseIndex);
				const FCompactPoseBoneIndex StoredCompactPoseIndex =  StoredBoneContainer.GetCompactPoseIndexFromSkeletonPoseIndex(SkeletonPoseIndex);
				if (StoredCompactPoseIndex == INDEX_NONE)
				{
					// If our stored pose doesn't have the bone, reset to ref pose.
					PoseContext.Pose[CompactPoseIndex] = CurrentBoneContainer.GetRefPoseTransform(CompactPoseIndex);
				}
				else
				{
					PoseContext.Pose[CompactPoseIndex] = StoredBones[StoredCompactPoseIndex.GetInt()];
				}
			}
		}
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
void FBlendStackAnimPlayer::UpdateSourceLinkNode()
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

bool FBlendStackAnimPlayer::IsLooping() const
{
	if (SequencePlayerNode.GetSequence())
	{
		return SequencePlayerNode.IsLooping();
	}

	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		return BlendSpacePlayerNode.IsLooping();
	}

	return false;
}

void FBlendStackAnimPlayer::Evaluate_AnyThread(FPoseContext& Output)
{
	if (SequencePlayerNode.GetSequence() || BlendSpacePlayerNode.GetBlendSpace())
	{
		UpdateSourceLinkNode();
		MirrorNode.Evaluate_AnyThread(Output);

		if (OverrideCurve.Num() != 0)
		{
			UE::Anim::FNamedValueArrayUtils::Union(Output.Curve, OverrideCurve);
		}
	}
	else
	{
		RestorePoseContext(Output);
	}
}

void FBlendStackAnimPlayer::Update_AnyThread(const FAnimationUpdateContext& Context)
{
	UpdateSourceLinkNode();
	MirrorNode.Update_AnyThread(Context);
}

float FBlendStackAnimPlayer::GetAccumulatedTime() const
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

float FBlendStackAnimPlayer::GetPlayRate() const
{
	if (SequencePlayerNode.GetSequence())
	{
		return SequencePlayerNode.GetPlayRate();
	}

	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		return BlendSpacePlayerNode.GetPlayRate();
	}

	return 0.f;
}

bool FBlendStackAnimPlayer::IsActive() const
{
	return TimeToActivation <= 0.f;
}

void FBlendStackAnimPlayer::UpdateWithDeltaTime(float DeltaTime, int32 PlayerDepth, float PlayerDepthBlendInTimeMultiplier)
{
	const bool bIsMainPlayer = PlayerDepth == 0;

	if (TimeToActivation > 0.f)
	{
		TimeToActivation -= DeltaTime;

		if (TimeToActivation < 0.f)
		{
			DeltaTime = -TimeToActivation;
			TimeToActivation = 0.f;
		}
		else
		{
			DeltaTime = 0.f;
		}
	}

	if (bIsMainPlayer)
	{
		CurrentBlendInTime += DeltaTime;
	}
	else
	{
		const float ScaledDeltaTime = DeltaTime * FMath::Pow(PlayerDepthBlendInTimeMultiplier, PlayerDepth + 1);
		CurrentBlendInTime += ScaledDeltaTime;
	}
}

FVector FBlendStackAnimPlayer::GetBlendParameters() const
{
	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		return BlendSpacePlayerNode.GetPosition();
	}

	return FVector::ZeroVector;
}

void FBlendStackAnimPlayer::SetBlendParameters(const FVector& BlendParameters)
{
	if (BlendSpacePlayerNode.GetBlendSpace())
	{
		BlendSpacePlayerNode.SetPosition(BlendParameters);
	}
}

FString FBlendStackAnimPlayer::GetAnimationName() const
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

UAnimationAsset* FBlendStackAnimPlayer::GetAnimationAsset() const
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

float FBlendStackAnimPlayer::GetBlendInPercentage() const
{
	if (FMath::IsNearlyZero(TotalBlendInTime))
	{
		return 1.f;
	}

	return FMath::Clamp(GetCurrentBlendInTime() / TotalBlendInTime, 0.f, 1.f);
}

int32 FBlendStackAnimPlayer::GetBlendInWeightsNum() const
{
	return TotalBlendInTimePerBone.Num();
}

void FBlendStackAnimPlayer::GetBlendInWeights(TArrayView<float> Weights) const
{
	check(Weights.Num() == GetBlendInWeightsNum());
	
	for (int32 BoneIdx = 0; BoneIdx < Weights.Num(); ++BoneIdx)
	{
		const float TotalBlendInTimeBoneIdx = TotalBlendInTimePerBone[BoneIdx];
		if (FMath::IsNearlyZero(TotalBlendInTimeBoneIdx))
		{
			Weights[BoneIdx] = 1.f;
		}
		else
		{
			const float UnclampedLinearWeight = GetCurrentBlendInTime() / TotalBlendInTimeBoneIdx;
			Weights[BoneIdx] = FAlphaBlend::AlphaToBlendOption(UnclampedLinearWeight, BlendOption);
		}
	}
}

/////////////////////////////////////////////////////
// FAnimNode_BlendStack_Standalone
void FAnimNode_BlendStack_Standalone::PopLastAnimPlayer()
{
	const int32 LastAnimPlayerIndex = AnimPlayers.Num() - 1;

#if DO_CHECK
	for (int32 AnimPlayerIndex = 0; AnimPlayerIndex < LastAnimPlayerIndex; ++AnimPlayerIndex)
	{
		// making sure only the last AnimPlayer can have a valid pose
		check(!AnimPlayers[AnimPlayerIndex].HasValidPoseContext());
	}
#endif // DO_CHECK

	if (LastAnimPlayerIndex > 0 && AnimPlayers[LastAnimPlayerIndex].HasValidPoseContext())
	{
		AnimPlayers[LastAnimPlayerIndex].MovePoseContextTo(AnimPlayers[LastAnimPlayerIndex - 1]);
	}

	// popping the last anim player
	AnimPlayers.SetNum(LastAnimPlayerIndex);
}

void FAnimNode_BlendStack_Standalone::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BlendStack_Evaluate_AnyThread);

	Super::Evaluate_AnyThread(Output);

	bool bDisableBlendStack = false;
#if ENABLE_ANIM_DEBUG
	bDisableBlendStack = !CVarAnimBlendStackEnable.GetValueOnAnyThread();

	const FVector& ActorLocation = Output.AnimInstanceProxy->GetActorTransform().GetTranslation();
	
	TStringBuilder<1024> MessageBuilder;

	for (int32 i = 0; i < AnimPlayers.Num(); ++i)
	{
		const FBlendStackAnimPlayer& AnimPlayer = AnimPlayers[i];
		MessageBuilder.Appendf(TEXT("%d) t:%.2f/%.2f a:%.2f m:%d %s\n"), 
			i, AnimPlayer.GetCurrentBlendInTime(), AnimPlayer.GetTotalBlendInTime(), AnimPlayer.GetTimeToActivation(),
			AnimPlayer.GetMirror() ? 1 : 0, *AnimPlayer.GetAnimationName());
	}

	FString VLogMessage(MessageBuilder.ToString());
	FString VLogCategoryName;
	VLogCategoryName.Appendf(TEXT("LogBlendStack_%d"), GetNodeIndex());
	UE_VLOG_LOCATION(Output.GetAnimInstanceObject(), *VLogCategoryName, Verbose, ActorLocation + FVector(0, 0, 100.0f), 0.f, FColor::Yellow, TEXT("%s"), *VLogMessage);
#endif // ENABLE_ANIM_DEBUG


	const int32 BlendStackSize = AnimPlayers.Num();
	if (BlendStackSize <= 0)
	{
		Output.ResetToRefPose();
	}
	else if (BlendStackSize == 1 || bDisableBlendStack)
	{
		EvaluateSample(Output, 0);
	}
	else
	{
		// evaluating the last AnimPlayer into Output...
		EvaluateSample(Output, BlendStackSize - 1);

		FPoseContext EvaluationPoseContext(Output);

		const USkeleton* SkeletonAsset = Output.AnimInstanceProxy->GetRequiredBones().GetSkeletonAsset();
		check(SkeletonAsset);

		const FReferenceSkeleton& RefSkeleton = SkeletonAsset->GetReferenceSkeleton();
		const int32 NumSkeletonBones = RefSkeleton.GetNum();

		auto EvaluateAndBlendPlayerByIndex = [this, &EvaluationPoseContext, &Output](int32 PlayerIndex)
		{
			FAnimationPoseData OutputAnimationPoseData(Output);
			FAnimationPoseData EvaluationAnimationPoseData(EvaluationPoseContext);

			// Evaluate into EvaluationPoseContext and then blend it with the Output (initialized with the last AnimPlayer evaluation)
			EvaluateSample(EvaluationPoseContext, PlayerIndex);

			const int32 BlendInWeightsNum = AnimPlayers[PlayerIndex].GetBlendInWeightsNum();
			if (BlendInWeightsNum > 0)
			{
				TArrayView<float> Weights((float*)FMemory_Alloca(BlendInWeightsNum * sizeof(float)), BlendInWeightsNum);
				AnimPlayers[PlayerIndex].GetBlendInWeights(Weights);
				BlendWithPosePerBone(OutputAnimationPoseData, EvaluationAnimationPoseData, Weights);
			}
			else
			{
				const float Weight = 1.f - FAlphaBlend::AlphaToBlendOption(AnimPlayers[PlayerIndex].GetBlendInPercentage(), AnimPlayers[PlayerIndex].GetBlendOption());
				BlendWithPose(OutputAnimationPoseData, EvaluationAnimationPoseData, Weight);
			}
		};

		// ...continuing with the valuation and accumulation on the Output FPoseContext
		// of AnimPlayer(s) from the second last to the AnimPlayer[MaxActiveBlends].
		int32 PlayerIndex = BlendStackSize - 2;
		// Start evaluating with our least significant players.
		for (; PlayerIndex >= MaxActiveBlends; --PlayerIndex)
		{
			EvaluateAndBlendPlayerByIndex(PlayerIndex);

			// too many AnimPlayers! we don't have enough available blends to hold them all, so we accumulate the blended poses into Output / BlendedPoseContext.
			PopLastAnimPlayer();
		}

		// At this point Output FPoseContext contains all the weighted accumulated poses of the from AnimPlayer[MaxActiveBlends] to AnimPlayer[AnimPlayer.Num()-1]
		if (PlayerIndex == (MaxActiveBlends - 1))
		{
			check(AnimPlayers.Num() == MaxActiveBlends + 1);

			if (bStoreBlendedPose)
			{
				// We store Output / BlendedPoseContext into the last AnimPlayer, that will hold a static pose, no longer an animation playing.
				AnimPlayers.Last().StorePoseContext(Output);
			}
#if !NO_LOGGING
			// warning if we're dropping an animplayer with relevant (MaxBlendInTimeToOverrideAnimation) weight (GetBlendInPercentage)
			else if (AnimPlayers.Last().GetBlendInPercentage() < (1.f - MaxBlendInTimeToOverrideAnimation))
			{
				UE_LOG(LogBlendStack, Warning, TEXT("FAnimNode_BlendStack_Standalone dropping animplayer with blend in at %.2f"), AnimPlayers.Last().GetBlendInPercentage());
			}
#endif // !NO_LOGGING
		}

		// Continue with the evaluation of the most significant AnimPlayer(s) with the associated graphs
		for (; PlayerIndex >= 0; --PlayerIndex)
		{
			EvaluateAndBlendPlayerByIndex(PlayerIndex);
		}
	}
}

void FAnimNode_BlendStack_Standalone::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	Super::Initialize_AnyThread(Context);

	if (bShouldFilterNotifies)
	{
		NotifiesFiredLastTick = MakeShared<TArray<FName>>();
		NotifyRecencyMap = MakeShared<TMap<FName, float>>();
	}
	
	Reset();

	if (PerSampleGraphPoseLinks.IsEmpty() == false)
	{
		SampleGraphExecutionHelpers.SetNum(PerSampleGraphPoseLinks.Num());
		for (UE::BlendStack::FBlendStack_SampleGraphExecutionHelper& ExecutionHelper : SampleGraphExecutionHelpers)
		{
			ExecutionHelper.CacheBoneCounter.Reset();
		}
	}
}

void FAnimNode_BlendStack_Standalone::CacheBones_AnyThread(const FAnimationCacheBonesContext& Context) 
{
	const int32 BlendStackSize = AnimPlayers.Num();
	for (int32 AnimPlayerIndex = 0; AnimPlayerIndex < BlendStackSize; ++AnimPlayerIndex)
	{
		// Cache bones for all active anim players.
		// There's no need to check for weight since all unneeded anim players 
		// would have been pruned during the last evaluation.
		CacheBonesForSample(Context, AnimPlayerIndex);
	}
}

void FAnimNode_BlendStack_Standalone::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_BlendStack_UpdateAssetPlayer);

	Super::UpdateAssetPlayer(Context);

	if (bShouldFilterNotifies)
	{
		if (const UWorld* World = Context.GetAnimInstanceObject()->GetWorld())
		{
			const double CurrentGameTime = World->GetTimeSeconds();
		
			// Set target time-outs for notifies fired last tick.
			for (const FName & NotifyName : *NotifiesFiredLastTick)
			{
				NotifyRecencyMap->FindOrAdd(NotifyName) = CurrentGameTime + NotifyRecencyTimeOut;
			}
			NotifiesFiredLastTick->Reset();
			
			// Find notifies that have timed-out and should be allowed to fire this tick.
			for (auto It = NotifyRecencyMap->CreateIterator(); It; ++It)
			{
				if (CurrentGameTime >= It->Value)
				{
					It.RemoveCurrent();
				}
			}
		}
	}
	
	UE::Anim::TOptionalScopedGraphMessage<UE::Anim::FBlendStackAnimEventsFilterScope> BlendStackInfoScope(bShouldFilterNotifies, Context, NotifiesFiredLastTick, NotifyRecencyMap);

	// AnimPlayers[0] is the most newly inserted AnimPlayer, AnimPlayers[AnimPlayers.Num()-1] is the oldest, so to calculate the weights
	// we ask AnimPlayers[0] its BlendInPercentage and then distribute the left over (CurrentWeightMultiplier) to the rest of the AnimPlayers
	// AnimPlayers[AnimPlayerIndex].GetBlendWeight() will now store the weighted contribution of AnimPlayers[AnimPlayerIndex] to be able to calculate root motion from animation
	float CurrentWeightMultiplier = 1.f;
	const int32 BlendStackSize = AnimPlayers.Num();
	int32 AnimPlayerIndex = 0;
	while (AnimPlayerIndex < BlendStackSize)
	{
		FBlendStackAnimPlayer& AnimPlayer = AnimPlayers[AnimPlayerIndex];
		const bool bIsLastAnimPlayers = AnimPlayerIndex == BlendStackSize - 1;
		const float BlendInPercentage = bIsLastAnimPlayers ? 1.f : AnimPlayer.GetBlendInPercentage();
		const float AnimPlayerBlendWeight = CurrentWeightMultiplier * BlendInPercentage;

		FAnimationUpdateContext AnimPlayerContext = Context.FractionalWeightAndRootMotion(AnimPlayerBlendWeight, AnimPlayerBlendWeight);
		UpdateSample((AnimPlayerIndex == 0) ? AnimPlayerContext : AnimPlayerContext.AsInactive(), AnimPlayerIndex);
		CurrentWeightMultiplier *= (1.f - BlendInPercentage);

		++AnimPlayerIndex;

		if (CurrentWeightMultiplier < UE_KINDA_SMALL_NUMBER)
		{
			break;
		}
	}

	// AnimPlayers[AnimPlayerIndex] is the first FBlendStackAnimPlayer with a weight contribution of zero,
	// so we can discard it and all the successive AnimPlayers as well
	// NoTe that it's safe to delete all those players, becasue we didn't call SamplePlayer.Update_AnyThread 
	// hence not register sequence / blendspace player InternalTimeAccumulator via FAnimTickRecord(s)
	const int32 WantedAnimPlayersNum = FMath::Max(1, AnimPlayerIndex); // we save at least one FBlendStackAnimPlayer
	while (AnimPlayers.Num() > WantedAnimPlayersNum)
	{
		PopLastAnimPlayer();
	}
}

bool FAnimNode_BlendStack_Standalone::IsSampleGraphAvailableForPlayer(const int32 PlayerIndex)
{
	// If we have any sample graphs, our player has been assigned a pose link index.
	// Players with a stored pose don't need to run the graph.
	return !PerSampleGraphPoseLinks.IsEmpty() && !AnimPlayers[PlayerIndex].HasValidPoseContext();
}

void FAnimNode_BlendStack_Standalone::EvaluateSample(FPoseContext& Output, const int32 PlayerIndex)
{
	FBlendStackAnimPlayer& SamplePlayer = AnimPlayers[PlayerIndex];
	// MaxActiveBlends == 0, means we're using inertialization. Run the the graph.
	const bool bIsSampleGraphAvailable = IsSampleGraphAvailableForPlayer(PlayerIndex);
	if (!bIsSampleGraphAvailable)
	{
		// If we have no sample graph, evaluate the player directly.
		SamplePlayer.Evaluate_AnyThread(Output);
		return;
	}

	const int32 SampleIndex = SamplePlayer.GetPoseLinkIndex();
	UE::BlendStack::FBlendStack_SampleGraphExecutionHelper& PoseLink = SampleGraphExecutionHelpers[SampleIndex];
	PoseLink.EvaluatePlayer(Output, SamplePlayer, PerSampleGraphPoseLinks[SampleIndex]);
}

void UE::BlendStack::FBlendStack_SampleGraphExecutionHelper::EvaluatePlayer(FPoseContext& Output, FBlendStackAnimPlayer& SamplePlayer, FPoseLink& SamplePoseLink)
{
	SetInputPosePlayer(SamplePlayer);

	// Make sure CacheBones has been called before evaluating.
	ConditionalCacheBones(Output, SamplePoseLink);
	// The anim player may or may not have its Evaluate_AnyThread called through the graph update. 
	SamplePoseLink.Evaluate(Output);
}

void UE::BlendStack::FBlendStack_SampleGraphExecutionHelper::ConditionalCacheBones(const FAnimationBaseContext& Context, FPoseLink& SamplePoseLink)
{
	// Only call CacheBones when needed.
	if (!CacheBoneCounter.IsSynchronized_Counter(Context.AnimInstanceProxy->GetCachedBonesCounter()))
	{
		// Keep track of samples that have had CacheBones called on.
		CacheBoneCounter.SynchronizeWith(Context.AnimInstanceProxy->GetCachedBonesCounter());

		FAnimationCacheBonesContext CacheBoneContext(Context.AnimInstanceProxy);
		SamplePoseLink.CacheBones(CacheBoneContext);
	}
}

void FAnimNode_BlendStack_Standalone::UpdateSample(const FAnimationUpdateContext& Context, const int32 PlayerIndex)
{
	FBlendStackAnimPlayer& SamplePlayer = AnimPlayers[PlayerIndex];

	if (SamplePlayer.IsActive())
	{
		const bool bHasSampleGraph = IsSampleGraphAvailableForPlayer(PlayerIndex);
		if (bHasSampleGraph)
		{
			const int32 SampleIndex = SamplePlayer.GetPoseLinkIndex();
			UE::BlendStack::FBlendStack_SampleGraphExecutionHelper& ExecutionHelper = SampleGraphExecutionHelpers[SampleIndex];
			ExecutionHelper.SetInputPosePlayer(SamplePlayer);
			// The anim player may or may not have its Update_AnyThread called through the graph update. 
			PerSampleGraphPoseLinks[SampleIndex].Update(Context);
		}
		else
		{
			// If we have no sample graph, update the player directly.
			SamplePlayer.Update_AnyThread(Context);
		}
	}

	// Advance the blend-in time regardless of whether or not the player was updated.
	SamplePlayer.UpdateWithDeltaTime(Context.GetDeltaTime(), PlayerIndex, PlayerDepthBlendInTimeMultiplier);
}

void FAnimNode_BlendStack_Standalone::CacheBonesForSample(const FAnimationCacheBonesContext& Context, const int32 PlayerIndex)
{
	FBlendStackAnimPlayer& SamplePlayer = AnimPlayers[PlayerIndex];
	const bool bHasSampleGraph = IsSampleGraphAvailableForPlayer(PlayerIndex);
	if (bHasSampleGraph)
	{
		const int32 SampleIndex = SamplePlayer.GetPoseLinkIndex();
		UE::BlendStack::FBlendStack_SampleGraphExecutionHelper& ExecutionHelper = SampleGraphExecutionHelpers[SampleIndex];
		ExecutionHelper.ConditionalCacheBones(Context, PerSampleGraphPoseLinks[SampleIndex]);
	}
}

void FAnimNode_BlendStack_Standalone::InitializeSample(const FAnimationInitializeContext& Context, FBlendStackAnimPlayer& SamplePlayer)
{
	if (SamplePlayer.GetPoseLinkIndex() != INDEX_NONE)
	{
		const int32 SampleIndex = SamplePlayer.GetPoseLinkIndex();
		FPoseLink& PoseLink = PerSampleGraphPoseLinks[SampleIndex];
		UE::BlendStack::FBlendStack_SampleGraphExecutionHelper& ExecutionHelper = SampleGraphExecutionHelpers[SampleIndex];
		ExecutionHelper.SetInputPosePlayer(SamplePlayer);
		PoseLink.Initialize(Context);
		ExecutionHelper.ConditionalCacheBones(Context, PoseLink);
	}
}

UAnimationAsset* FAnimNode_BlendStack_Standalone::GetAnimAsset() const
{
	return AnimPlayers.IsEmpty() ? nullptr : AnimPlayers[0].GetAnimationAsset();
}

float FAnimNode_BlendStack_Standalone::GetAccumulatedTime() const
{
	return AnimPlayers.IsEmpty() ? 0.f : AnimPlayers[0].GetAccumulatedTime();
}

static void RequestInertialBlend(const FAnimationUpdateContext& Context, float BlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption BlendOption)
{
#if ENABLE_ANIM_DEBUG
	if (!CVarAnimBlendStackEnable.GetValueOnAnyThread())
	{
		return;
	}
#endif // ENABLE_ANIM_DEBUG

	if (BlendTime > 0.0f)
	{
		UE::Anim::IInertializationRequester* InertializationRequester = Context.GetMessage<UE::Anim::IInertializationRequester>();
		if (InertializationRequester)
		{
			FInertializationRequest Request;
			Request.Duration = BlendTime;
			Request.BlendProfile = BlendProfile;
			Request.bUseBlendMode = true;
			Request.BlendMode = BlendOption;
#if ANIM_TRACE_ENABLED
			Request.NodeId = Context.GetCurrentNodeId();
			Request.AnimInstance = Context.AnimInstanceProxy->GetAnimInstanceObject();
#endif

			InertializationRequester->RequestInertialization(Request);
		}
		else
		{
			FAnimNode_Inertialization::LogRequestError(Context, Context.GetCurrentNodeId());
		}
	}
}

void FAnimNode_BlendStack_Standalone::BlendTo(const FAnimationUpdateContext& Context, UAnimationAsset* AnimationAsset, float AccumulatedTime, bool bLoop,
	bool bMirrored, UMirrorDataTable* MirrorDataTable, float BlendTime, const UBlendProfile* BlendProfile, EAlphaBlendOption BlendOption, bool bUseInertialBlend,
	const FVector& BlendParameters, float PlayRate, float ActivationDelay, FName GroupName, EAnimGroupRole::Type GroupRole, EAnimSyncMethod GroupMethod)
{
	const bool bBlendStackIsEmpty = AnimPlayers.IsEmpty();

	// If the blend stack is empty, we shouldn't blend. Pop into the requested pose.
	if (bBlendStackIsEmpty)
	{		
		BlendTime = 0.0f;
	}

	if (bUseInertialBlend)
	{
		RequestInertialBlend(Context, BlendTime, BlendProfile, BlendOption);
		BlendTime = 0.0f;
	}

	// If we don't add a new player, re-use the same graph...
	int32 NewSamplePoseLinkIndex = CurrentSamplePoseLink;
	if (!bBlendStackIsEmpty &&  AnimPlayers[0].GetBlendInPercentage() < 1.0f &&
		AnimPlayers[0].GetCurrentBlendInTime() < MaxBlendInTimeToOverrideAnimation &&
		FMath::IsNearlyEqual(AnimPlayers[0].GetTimeToActivation(), ActivationDelay))
	{
		// replacing AnimPlayers[0] with this new BlendTo request
		UE_LOG(LogBlendStack, Verbose, TEXT("FAnimNode_BlendStack_Standalone '%s' replaced by '%s' because blend time in is less than MaxBlendInTimeToOverrideAnimation (%.2f / %.2f)"), *AnimPlayers[0].GetAnimationName(), *GetNameSafe(AnimationAsset), AnimPlayers[0].GetCurrentBlendInTime(), MaxBlendInTimeToOverrideAnimation);
	}
	else if (AnimPlayers.Num() <= MaxActiveBlends + 2)
	{
		AnimPlayers.Insert(FBlendStackAnimPlayer(), 0);
		// ...otherwise, assign a new graph.
		NewSamplePoseLinkIndex = GetNextPoseLinkIndex();
	}
	else
	{
		// else it means we had multiple BlendTo suring the same frame. we'll let the last one win
		UE_LOG(LogBlendStack, Warning, TEXT("FAnimNode_BlendStack_Standalone multiple BlendTo requests during the same frame: only the last request will be put on this BlendStack"));
	}

	FBlendStackAnimPlayer& AnimPlayer = AnimPlayers[0];

	FAnimationInitializeContext InitContext(Context.AnimInstanceProxy, Context.SharedContext);
	AnimPlayer.Initialize(InitContext, AnimationAsset, AccumulatedTime, bLoop, bMirrored, MirrorDataTable, BlendTime, BlendProfile, BlendOption, BlendParameters, PlayRate, ActivationDelay, NewSamplePoseLinkIndex, GroupName, GroupRole, GroupMethod);
	InitializeSample(InitContext, AnimPlayer);
}

void FAnimNode_BlendStack_Standalone::Reset()
{
	// reserving MaxActiveBlends + 2 AnimPlayers, to avoid any reallocation
	AnimPlayers.Reserve(MaxActiveBlends + 2);
	AnimPlayers.Reset();

	if (bShouldFilterNotifies)
	{
		NotifiesFiredLastTick->Reset();
		NotifyRecencyMap->Reset();
	}
}

int32 FAnimNode_BlendStack_Standalone::GetNextPoseLinkIndex()
{
	if (PerSampleGraphPoseLinks.IsEmpty())
	{
		return INDEX_NONE;
	}

	const int32 NumPoseLinks = PerSampleGraphPoseLinks.Num();
	++CurrentSamplePoseLink;
	if (CurrentSamplePoseLink == NumPoseLinks) { CurrentSamplePoseLink = 0; }

	return CurrentSamplePoseLink;
}

void FAnimNode_BlendStack_Standalone::UpdatePlayRate(float PlayRate)
{
	if (!AnimPlayers.IsEmpty())
	{
		AnimPlayers[0].UpdatePlayRate(PlayRate);
	}
}

void FAnimNode_BlendStack_Standalone::GatherDebugData(FNodeDebugData& DebugData)
{
#if ENABLE_ANIM_DEBUG
	DebugData.AddDebugItem(FString::Printf(TEXT("%s"), *DebugData.GetNodeName(this)));
	for (int32 i = 0; i < AnimPlayers.Num(); ++i)
	{
		const FBlendStackAnimPlayer& AnimPlayer = AnimPlayers[i];
		DebugData.AddDebugItem(FString::Printf(TEXT("%d) t:%.2f/%.2f a:%.2f m:%d %s"),
				i, AnimPlayer.GetCurrentBlendInTime(), AnimPlayer.GetTotalBlendInTime(), AnimPlayer.GetTimeToActivation(),
				AnimPlayer.GetMirror() ? 1 : 0, *AnimPlayer.GetAnimationName()));
	}
#endif // ENABLE_ANIM_DEBUG

	// propagating GatherDebugData to the AnimPlayers
	for (FBlendStackAnimPlayer& AnimPlayer : AnimPlayers)
	{
		AnimPlayer.GetMirrorNode().GatherDebugData(DebugData);
	}
}

// this function is the optimized version of 
// FAnimationRuntime::BlendTwoPosesTogetherPerBone(InOutPoseData, OtherPoseData, OtherPoseWeights, InOutPoseData);
void FAnimNode_BlendStack_Standalone::BlendWithPosePerBone(FAnimationPoseData& InOutPoseData, const FAnimationPoseData& OtherPoseData, TConstArrayView<float> OtherPoseWeights)
{
	using namespace UE::Anim;

	FCompactPose& InOutPose = InOutPoseData.GetPose();
	FBlendedCurve& InOutCurve = InOutPoseData.GetCurve();
	FStackAttributeContainer& InOutAttributes = InOutPoseData.GetAttributes();

	const FCompactPose& OtherPose = OtherPoseData.GetPose();

	for (FCompactPoseBoneIndex BoneIndex : InOutPose.ForEachBoneIndex())
	{
		const float OtherPoseBoneWeight = OtherPoseWeights[BoneIndex.GetInt()];
		if (FAnimationRuntime::IsFullWeight(OtherPoseBoneWeight))
		{
			InOutPose[BoneIndex] = OtherPose[BoneIndex];
		}
		else if (FAnimationRuntime::HasWeight(OtherPoseBoneWeight))
		{
			const ScalarRegister VInOutPoseBoneWeight(1.f - OtherPoseBoneWeight);
			const ScalarRegister VOtherPoseBoneWeight(OtherPoseBoneWeight);

			InOutPose[BoneIndex] *= VInOutPoseBoneWeight;
			InOutPose[BoneIndex].AccumulateWithShortestRotation(OtherPose[BoneIndex], VOtherPoseBoneWeight);
		}
		// else we leave InOutPose[BoneIndex] as is
	}

	// Ensure that all of the resulting rotations are normalized
	InOutPose.NormalizeRotations();

	// @note : This isn't perfect as curve can link to joint, and it would be the best to use that information
	// but that is very expensive option as we have to have another indirect look up table to search. 
	// For now, replacing with combine (non-zero will be overriden)
	// in the future, we might want to do this outside if we want per bone blend to apply curve also UE-39182
	InOutPoseData.GetCurve().Combine(OtherPoseData.GetCurve());

	// @todo: optimize away the copy
	FStackAttributeContainer CustomAttributes;
	Attributes::BlendAttributesPerBone(InOutAttributes, OtherPoseData.GetAttributes(), OtherPoseWeights, CustomAttributes);
	InOutAttributes = CustomAttributes;
}

// this function is the optimized version of 
// FAnimationRuntime::BlendTwoPosesTogether(InOutPoseData, OtherPoseData, InOutPoseWeight, InOutPoseData);
void FAnimNode_BlendStack_Standalone::BlendWithPose(FAnimationPoseData& InOutPoseData, const FAnimationPoseData& OtherPoseData, const float InOutPoseWeight)
{
	using namespace UE::Anim;

	FCompactPose& InOutPose = InOutPoseData.GetPose();
	FBlendedCurve& InOutCurve = InOutPoseData.GetCurve();
	FStackAttributeContainer& InOutAttributes = InOutPoseData.GetAttributes();

	const FCompactPose& OtherPose = OtherPoseData.GetPose();

	const float OtherPoseWeight = 1.f - InOutPoseWeight;

	// @todo: reimplement the ispc version of this if needed
	const ScalarRegister VInOutPoseWeight(InOutPoseWeight);
	const ScalarRegister VOtherPoseWeight(OtherPoseWeight);

	for (FCompactPoseBoneIndex BoneIndex : InOutPose.ForEachBoneIndex())
	{
		InOutPose[BoneIndex] *= VInOutPoseWeight;
		InOutPose[BoneIndex].AccumulateWithShortestRotation(OtherPose[BoneIndex], VOtherPoseWeight);
	}

	// Ensure that all of the resulting rotations are normalized
	InOutPose.NormalizeRotations();

	InOutCurve.LerpTo(OtherPoseData.GetCurve(), OtherPoseWeight);
	
	// @todo: optimize away the copy
	FStackAttributeContainer CustomAttributes;
	Attributes::BlendAttributes({ InOutAttributes, OtherPoseData.GetAttributes() }, { InOutPoseWeight, OtherPoseWeight }, { 0, 1 }, CustomAttributes);
	InOutAttributes = CustomAttributes;
}

/////////////////////////////////////////////////////
// FAnimNode_BlendStack

void FAnimNode_BlendStack::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	const bool bNeedsReset =
		bResetOnBecomingRelevant &&
		UpdateCounter.HasEverBeenUpdated() &&
		!UpdateCounter.WasSynchronizedCounter(Context.AnimInstanceProxy->GetUpdateCounter());

	if (bNeedsReset)
	{
		Reset();
		bForceBlendNextUpdate = false;
	}

	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());

	GetEvaluateGraphExposedInputs().Execute(Context);

	bool bExecuteBlendTo = false;
	if (AnimationAsset == nullptr && !bForceBlendNextUpdate)
	{
		bExecuteBlendTo = false;
	}
	else if (AnimPlayers.IsEmpty())
	{
		bExecuteBlendTo = true;
	}
	else
	{
		const FBlendStackAnimPlayer& MainAnimPlayer = AnimPlayers[0];
		const UAnimationAsset* PlayingAnimationAsset = MainAnimPlayer.GetAnimationAsset();

		if (bForceBlendNextUpdate)
		{
			bForceBlendNextUpdate = false;
			bExecuteBlendTo = true;
		}
		else if (AnimationAsset != PlayingAnimationAsset)
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
		BlendTo(Context, AnimationAsset, AnimationTime, bLoop, bMirrored, MirrorDataTable.Get(),
			BlendTime, BlendProfile, BlendOption, bUseInertialBlend, BlendParameters, WantedPlayRate);
	}

	const bool bDidBlendToRequestAnInertialBlend = bExecuteBlendTo && bUseInertialBlend;
	UE::Anim::TOptionalScopedGraphMessage<UE::Anim::FAnimInertializationSyncScope> InertializationSync(bDidBlendToRequestAnInertialBlend, Context);
	
	UpdatePlayRate(WantedPlayRate);
	Super::UpdateAssetPlayer(Context);
}

void FAnimNode_BlendStack::ForceBlendNextUpdate()
{
	bForceBlendNextUpdate = true;
}

void UE::BlendStack::FBlendStack_SampleGraphExecutionHelper::SetInputPosePlayer(FBlendStackAnimPlayer& InPlayer)
{
	// Because our anim players may get reallocated, or change indices due to push/pops,
	// we must call this before every operation that might end up needing the anim player through the graph's input nodes.
	Player = &InPlayer;
}

#undef LOCTEXT_NAMESPACE
