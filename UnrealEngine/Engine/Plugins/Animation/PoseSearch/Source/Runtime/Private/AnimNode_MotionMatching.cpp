// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_MotionMatching.h"

#include "Animation/AnimInertializationSyncScope.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimStats.h"
#include "Animation/BlendSpace.h"
#include "Components/SkeletalMeshComponent.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/Trace/PoseSearchTraceLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_MotionMatching)

#define LOCTEXT_NAMESPACE "AnimNode_MotionMatching"

#if ENABLE_ANIM_DEBUG
static TAutoConsoleVariable<bool> CVarAnimNodeMotionMatchingDrawQuery(TEXT("a.AnimNode.MotionMatching.DebugDrawQuery"), false, TEXT("Draw input query"));
static TAutoConsoleVariable<bool> CVarAnimNodeMotionMatchingDrawCurResult(TEXT("a.AnimNode.MotionMatching.DebugDrawCurResult"), false, TEXT("Draw current result"));
static TAutoConsoleVariable<bool> CVarAnimNodeMotionMatchingDrawInfo(TEXT("a.AnimNode.MotionMatching.DebugDrawInfo"), false, TEXT("Draw info like current databases and asset"));
static TAutoConsoleVariable<bool> CVarAnimNodeMotionMatchingDrawInfoVerbose(TEXT("a.AnimNode.MotionMatching.DebugDrawInfoVerbose"), true, TEXT("Draw additional info like blend stack"));
static TAutoConsoleVariable<float> CVarAnimNodeMotionMatchingDrawInfoHeight(TEXT("a.AnimNode.MotionMatching.DebugDrawInfoHeight"), 50.f, TEXT("Vertical offset for DebugDrawInfo"));
#endif

/////////////////////////////////////////////////////
// FAnimNode_MotionMatching

void FAnimNode_MotionMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	GetEvaluateGraphExposedInputs().Execute(Context);

	FAnimNode_BlendStack_Standalone::Initialize_AnyThread(Context);
	MotionMatchingState.Reset(Context.AnimInstanceProxy->GetComponentTransform());
}

void FAnimNode_MotionMatching::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(MotionMatching, !IsInGameThread());

	using namespace UE::PoseSearch;

	FAnimNode_BlendStack_Standalone::Evaluate_AnyThread(Output);

	const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();
	FTransform RootMotionTransformDelta;
	if (RootMotionProvider && RootMotionProvider->HasRootMotion(Output.CustomAttributes))
	{
		RootMotionProvider->ExtractRootMotion(Output.CustomAttributes, RootMotionTransformDelta);
	}
	else
	{
		RootMotionTransformDelta = FTransform::Identity;
		RootMotionProvider = nullptr;
	}

	// applying MotionMatchingState.ComponentDeltaYaw (considered as root bone delta yaw) to the root bone and the root motion delta transform
	if (!FMath::IsNearlyZero(MotionMatchingState.ComponentDeltaYaw))
	{
		const FQuat RootBoneDelta(FRotator(0.f, MotionMatchingState.ComponentDeltaYaw, 0.f));
		FCompactPoseBoneIndex RootBoneIndex(RootBoneIndexType);

		// @todo: this code assumes the root bone from animation is always identity. Implement if not the case as
		//		Output.Pose[RootBoneIndex].SetRotation(Output.Pose[RootBoneIndex].GetRotation() * RootBoneDelta);
		//		Output.Pose[RootBoneIndex].NormalizeRotation();
		// 		etc etc
		check(Output.Pose[RootBoneIndex].GetRotation().IsIdentity());

		Output.Pose[RootBoneIndex].SetRotation(RootBoneDelta);
		RootMotionTransformDelta.SetTranslation(RootBoneDelta.RotateVector(RootMotionTransformDelta.GetTranslation()));

		if (RootMotionProvider)
		{
			RootMotionProvider->OverrideRootMotion(RootMotionTransformDelta, Output.CustomAttributes);
		}
	}

	MotionMatchingState.AnimationDeltaYaw = FRotator(RootMotionTransformDelta.GetRotation()).Yaw;

#if UE_POSE_SEARCH_TRACE_ENABLED
	MotionMatchingState.RootMotionTransformDelta = RootMotionTransformDelta;
#endif //UE_POSE_SEARCH_TRACE_ENABLED
}

void FAnimNode_MotionMatching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(UpdateAssetPlayer);

	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_UpdateAssetPlayer);

	using namespace UE::PoseSearch;

	GetEvaluateGraphExposedInputs().Execute(Context);

	bool bNeedsReset =
		bResetOnBecomingRelevant &&
		UpdateCounter.HasEverBeenUpdated() &&
		!UpdateCounter.WasSynchronizedCounter(Context.AnimInstanceProxy->GetUpdateCounter());

#if WITH_EDITOR
	if (EAsyncBuildIndexResult::Success != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(MotionMatchingState.CurrentSearchResult.Database.Get(), ERequestAsyncBuildFlag::ContinueRequest))
	{
		bNeedsReset = true;
	}
	// in case this node is not updated, and MotionMatchingState.CurrentSearchResult.Database gets modified, we could end up with CurrentSearchResult being out of synch with the updated database, so we need to reset the state
	else if (MotionMatchingState.CurrentSearchResult.IsValid() && MotionMatchingState.CurrentSearchResult.PoseIdx >= MotionMatchingState.CurrentSearchResult.Database->GetSearchIndex().GetNumPoses())
	{
		bNeedsReset = true;
	}
#endif // WITH_EDITOR

	// If we just became relevant and haven't been initialized yet, then reset motion matching state, otherwise update the asset time using the player node.
	if (bNeedsReset)
	{
		MotionMatchingState.Reset(Context.AnimInstanceProxy->GetComponentTransform());
		FAnimNode_BlendStack_Standalone::Reset();
	}
	else
	{
		// We adjust the motion matching state asset time to the current player node's asset time. This is done 
		// because the player node may have ticked more or less time than we expected due to variable dt or the 
		// dynamic playback rate adjustment and as such the motion matching state does not update by itself
		MotionMatchingState.AdjustAssetTime(GetAccumulatedTime());
	}
	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());

	// If the Database property hasn't been overridden, set it as the only database to search.
	if (!bOverrideDatabaseInput && Database)
	{
		DatabasesToSearch.Reset(1);
		DatabasesToSearch.Add(Database);
	}

#if ENABLE_ANIM_DEBUG
	if (CVarAnimNodeMotionMatchingDrawInfo.GetValueOnAnyThread())
	{
		const UPoseSearchDatabase* CurrentDatabase = MotionMatchingState.CurrentSearchResult.Database.Get();
		const UAnimationAsset* CurrentAnimationAsset = AnimPlayers.IsEmpty() ? nullptr : AnimPlayers[0].GetAnimationAsset();

		FString DebugInfo = FString::Printf(TEXT("NextUpdateInterruptMode(%s)\n"), *UEnum::GetValueAsString(NextUpdateInterruptMode));
		DebugInfo += FString::Printf(TEXT("Current Database(%s)\n"), *GetNameSafe(CurrentDatabase));
		DebugInfo += FString::Printf(TEXT("Current Asset(%s)\n"), *GetNameSafe(CurrentAnimationAsset));
		if (CVarAnimNodeMotionMatchingDrawInfoVerbose.GetValueOnAnyThread())
		{
			DebugInfo += FString::Printf(TEXT("Databases to search:\n"));
			for (const UPoseSearchDatabase* DatabaseToSearch : DatabasesToSearch)
			{
				DebugInfo += FString::Printf(TEXT("  %s\n"), *GetNameSafe(DatabaseToSearch));
			}
			DebugInfo += FString::Printf(TEXT("Blend Stack:\n"));
			for (const FBlendStackAnimPlayer& AnimPlayer : AnimPlayers)
			{
				DebugInfo += FString::Printf(TEXT("  %s [time:%.2f|playrate:%.2f]\n"), *GetNameSafe(AnimPlayer.GetAnimationAsset()), AnimPlayer.GetAccumulatedTime(), AnimPlayer.GetPlayRate());
			}
		}
		Context.AnimInstanceProxy->AnimDrawDebugInWorldMessage(DebugInfo, FVector::UpVector * CVarAnimNodeMotionMatchingDrawInfoHeight.GetValueOnAnyThread(), FColor::Yellow, 1.f /*TextScale*/);
	}
#endif // ENABLE_ANIM_DEBUG

	// Execute core motion matching algorithm
	UPoseSearchLibrary::UpdateMotionMatchingState(
		Context,
		DatabasesToSearch,
		BlendTime,
		MaxActiveBlends,
		PoseJumpThresholdTime,
		PoseReselectHistory,
		SearchThrottleTime,
		PlayRate,
		MotionMatchingState,
		NextUpdateInterruptMode,
		bShouldSearch,
		bShouldUseCachedChannelData
		#if ENABLE_ANIM_DEBUG
		, CVarAnimNodeMotionMatchingDrawQuery.GetValueOnAnyThread()
		, CVarAnimNodeMotionMatchingDrawCurResult.GetValueOnAnyThread()
		#endif // ENABLE_ANIM_DEBUG
	);

	UE::Anim::FNodeFunctionCaller::CallFunction(GetOnUpdateMotionMatchingStateFunction(), Context, *this);

	// If a new pose is requested, blend into the new asset via BlendStackNode
	if (MotionMatchingState.bJumpedToPose)
	{
		const FSearchIndexAsset* SearchIndexAsset = MotionMatchingState.CurrentSearchResult.GetSearchIndexAsset();
		const UPoseSearchDatabase* CurrentResultDatabase = MotionMatchingState.CurrentSearchResult.Database.Get();
		if (SearchIndexAsset && CurrentResultDatabase && CurrentResultDatabase->Schema)
		{
			const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = CurrentResultDatabase->GetAnimationAssetBase(*SearchIndexAsset);
			check(DatabaseAsset);

			if (UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(DatabaseAsset->GetAnimationAsset()))
			{
				FAnimNode_BlendStack_Standalone::BlendTo(Context, AnimationAsset, MotionMatchingState.CurrentSearchResult.AssetTime,
					SearchIndexAsset->IsLooping(), SearchIndexAsset->IsMirrored(), CurrentResultDatabase->Schema->GetMirrorDataTable(DefaultRole), BlendTime,
					BlendProfile, BlendOption, bUseInertialBlend, SearchIndexAsset->GetBlendParameters(), MotionMatchingState.WantedPlayRate);
			}
			else
			{
				checkNoEntry();
			}
		}
	}

	const bool bDidBlendToRequestAnInertialBlend = MotionMatchingState.bJumpedToPose && bUseInertialBlend;
	UE::Anim::TOptionalScopedGraphMessage<UE::Anim::FAnimInertializationSyncScope> InertializationSync(bDidBlendToRequestAnInertialBlend, Context);
	
	FAnimNode_BlendStack_Standalone::UpdatePlayRate(MotionMatchingState.WantedPlayRate);
	FAnimNode_BlendStack_Standalone::UpdateAssetPlayer(Context);

	NextUpdateInterruptMode = EPoseSearchInterruptMode::DoNotInterrupt;
}

const FAnimNodeFunctionRef& FAnimNode_MotionMatching::GetOnUpdateMotionMatchingStateFunction() const
{
	return GET_ANIM_NODE_DATA(FAnimNodeFunctionRef, OnMotionMatchingStateUpdated);
}

void FAnimNode_MotionMatching::SetDatabaseToSearch(UPoseSearchDatabase* InDatabase, EPoseSearchInterruptMode InterruptMode)
{
	SetDatabasesToSearch(MakeArrayView(&InDatabase, 1), InterruptMode);
}

FVector FAnimNode_MotionMatching::GetEstimatedFutureRootMotionVelocity() const
{
	return MotionMatchingState.GetEstimatedFutureRootMotionVelocity();
}

void FAnimNode_MotionMatching::SetDatabasesToSearch(TConstArrayView<UPoseSearchDatabase*> InDatabases, EPoseSearchInterruptMode InterruptMode)
{
	DatabasesToSearch.Reset();
	for (UPoseSearchDatabase* InDatabase : InDatabases)
	{
		DatabasesToSearch.AddUnique(InDatabase);
	}
	NextUpdateInterruptMode = InterruptMode;
	bOverrideDatabaseInput = true;
}

void FAnimNode_MotionMatching::ResetDatabasesToSearch(EPoseSearchInterruptMode InterruptMode)
{
	DatabasesToSearch.Reset();
	bOverrideDatabaseInput = false;
	NextUpdateInterruptMode = InterruptMode;
}

void FAnimNode_MotionMatching::SetInterruptMode(EPoseSearchInterruptMode InterruptMode)
{
	NextUpdateInterruptMode = InterruptMode;
}

// FAnimNode_AssetPlayerBase interface
bool FAnimNode_MotionMatching::GetIgnoreForRelevancyTest() const
{
	return GET_ANIM_NODE_DATA(bool, bIgnoreForRelevancyTest);
}

bool FAnimNode_MotionMatching::SetIgnoreForRelevancyTest(bool bInIgnoreForRelevancyTest)
{
#if WITH_EDITORONLY_DATA
	bIgnoreForRelevancyTest = bInIgnoreForRelevancyTest;
#endif

	if(bool* bIgnoreForRelevancyTestPtr = GET_INSTANCE_ANIM_NODE_DATA_PTR(bool, bIgnoreForRelevancyTest))
	{
		*bIgnoreForRelevancyTestPtr = bInIgnoreForRelevancyTest;
		return true;
	}

	return false;
}

#undef LOCTEXT_NAMESPACE
