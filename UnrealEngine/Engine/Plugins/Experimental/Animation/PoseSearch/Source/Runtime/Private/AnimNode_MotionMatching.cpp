// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_MotionMatching.h"

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
#include "Trace/PoseSearchTraceLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNode_MotionMatching)

#define LOCTEXT_NAMESPACE "AnimNode_MotionMatching"

#if ENABLE_ANIM_DEBUG
static TAutoConsoleVariable<int32> CVarAnimNodeMotionMatchingDrawQuery(TEXT("a.AnimNode.MotionMatching.DebugDrawQuery"), 0, TEXT("Draw input query"));
static TAutoConsoleVariable<int32> CVarAnimNodeMotionMatchingDrawCurResult(TEXT("a.AnimNode.MotionMatching.DebugDrawCurResult"), 0, TEXT("Draw current result"));
static TAutoConsoleVariable<int32> CVarAnimNodeMotionMatchingDrawInfo(TEXT("a.AnimNode.MotionMatching.DebugDrawInfo"), 0, TEXT("Draw info like current databases to search"));
static TAutoConsoleVariable<float> CVarAnimNodeMotionMatchingDrawInfoHeight(TEXT("a.AnimNode.MotionMatching.DebugDrawInfoHeight"), 50.f, TEXT("Vertical offset for DebugDrawInfo"));
#endif

/////////////////////////////////////////////////////
// FAnimNode_MotionMatching

void FAnimNode_MotionMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	GetEvaluateGraphExposedInputs().Execute(Context);

	BlendStackNode.Initialize_AnyThread(Context);

	Source.SetLinkNode(&BlendStackNode);
	Source.Initialize(Context);

	MotionMatchingState.UpdateRootBoneControl(Context.AnimInstanceProxy, 0.f);
}

void FAnimNode_MotionMatching::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);
	ANIM_MT_SCOPE_CYCLE_COUNTER_VERBOSE(MotionMatching, !IsInGameThread());

	Source.Evaluate(Output);

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
		Output.Pose[RootBoneIndex].SetRotation(Output.Pose[RootBoneIndex].GetRotation() * RootBoneDelta);
		Output.Pose[RootBoneIndex].NormalizeRotation();

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
	if (!FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(MotionMatchingState.CurrentSearchResult.Database.Get(), ERequestAsyncBuildFlag::ContinueRequest))
	{
		bNeedsReset = true;
	}
#endif // WITH_EDITOR

	// If we just became relevant and haven't been initialized yet, then reset motion matching state, otherwise update the asset time using the player node.
	if (bNeedsReset)
	{
		MotionMatchingState.Reset(Context.AnimInstanceProxy->GetComponentTransform());
	}
	else
	{
		// We adjust the motion matching state asset time to the current player node's asset time. This is done 
		// because the player node may have ticked more or less time than we expected due to variable dt or the 
		// dynamic playback rate adjustment and as such the motion matching state does not update by itself
		MotionMatchingState.AdjustAssetTime(BlendStackNode.GetAccumulatedTime());
	}
	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());

	// If the Database property hasn't been overridden, set it as the only database to search.
	if (!bOverrideDatabaseInput && Database)
	{
		DatabasesToSearch.Reset(1);
		DatabasesToSearch.Add(Database);
	}

#if ENABLE_ANIM_DEBUG
	if (CVarAnimNodeMotionMatchingDrawInfo.GetValueOnAnyThread() > 0)
	{
		FString DebugInfo = FString::Printf(TEXT("bForceInterruptNextUpdate(%d)\n"), bForceInterruptNextUpdate);
		DebugInfo += FString::Printf(TEXT("Current Database(%s)\n"), *GetNameSafe(MotionMatchingState.CurrentSearchResult.Database.Get()));
		DebugInfo += FString::Printf(TEXT("Databases to search:\n"));
		for (const UPoseSearchDatabase* DatabaseToSearch : DatabasesToSearch)
		{
			DebugInfo += FString::Printf(TEXT("  %s\n"), *GetNameSafe(DatabaseToSearch));
		}
		Context.AnimInstanceProxy->AnimDrawDebugInWorldMessage(DebugInfo, FVector::UpVector * CVarAnimNodeMotionMatchingDrawInfoHeight.GetValueOnAnyThread(), FColor::Yellow, 1.f /*TextScale*/);
	}
#endif // ENABLE_ANIM_DEBUG

	// Execute core motion matching algorithm
	UPoseSearchLibrary::UpdateMotionMatchingState(
		Context,
		DatabasesToSearch,
		Trajectory,
		TrajectorySpeedMultiplier,
		BlendTime,
		MaxActiveBlends,
		PoseJumpThresholdTime,
		PoseReselectHistory,
		SearchThrottleTime,
		PlayRate,
		MotionMatchingState,
		YawFromAnimationBlendRate,
		YawFromAnimationTrajectoryBlendTime,
		bForceInterruptNextUpdate,
		bShouldSearch
		#if ENABLE_ANIM_DEBUG
		, CVarAnimNodeMotionMatchingDrawQuery.GetValueOnAnyThread() > 0
		, CVarAnimNodeMotionMatchingDrawCurResult.GetValueOnAnyThread() > 0
		#endif // ENABLE_ANIM_DEBUG
	);

	// If a new pose is requested, blend into the new asset via BlendStackNode
	if (MotionMatchingState.bJumpedToPose)
	{
		const FSearchIndexAsset* SearchIndexAsset = MotionMatchingState.CurrentSearchResult.GetSearchIndexAsset();
		const UPoseSearchDatabase* CurrentResultDatabase = MotionMatchingState.CurrentSearchResult.Database.Get();
		if (SearchIndexAsset && CurrentResultDatabase && CurrentResultDatabase->Schema)
		{
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = CurrentResultDatabase->GetAnimationAssetBase(*SearchIndexAsset))
			{
				// root bone blending needs to be immediate if MM node controls the offset between mesh component and root bone
				const float RootBoneBlendTime = YawFromAnimationBlendRate < 0.f ? BlendTime : 0.f;
				BlendStackNode.BlendTo(DatabaseAsset->GetAnimationAsset(), MotionMatchingState.CurrentSearchResult.AssetTime,
					DatabaseAsset->IsLooping(), SearchIndexAsset->bMirrored, CurrentResultDatabase->Schema->MirrorDataTable.Get(),
					MaxActiveBlends, BlendTime, RootBoneBlendTime, BlendProfile, BlendOption, SearchIndexAsset->BlendParameters, MotionMatchingState.WantedPlayRate);
			}
		}
	}
	BlendStackNode.UpdatePlayRate(MotionMatchingState.WantedPlayRate);

	Source.Update(Context);

	bForceInterruptNextUpdate = false;
}

void FAnimNode_MotionMatching::GatherDebugData(FNodeDebugData& DebugData)
{
	Source.GatherDebugData(DebugData);
}

void FAnimNode_MotionMatching::SetDatabaseToSearch(UPoseSearchDatabase* InDatabase, bool bForceInterruptIfNew)
{
	if (DatabasesToSearch.Num() == 1 && DatabasesToSearch[0] == InDatabase)
	{
		UE_LOG(LogPoseSearch, Verbose, TEXT("FAnimNode_MotionMatching::SetDatabaseToSearch - Database(%s) is already set."), *GetNameSafe(InDatabase));
	}
	else
	{
		DatabasesToSearch.Reset();
		bOverrideDatabaseInput = false;
		if (InDatabase)
		{
			DatabasesToSearch.Add(InDatabase);
			bOverrideDatabaseInput = true;
		}

		bForceInterruptNextUpdate |= bForceInterruptIfNew;

		UE_LOG(LogPoseSearch, Verbose, TEXT("FAnimNode_MotionMatching::SetDatabaseToSearch - Setting to Database(%s), bForceInterruptIfNew(%d)."), *GetNameSafe(InDatabase), bForceInterruptIfNew);
	}
}

void FAnimNode_MotionMatching::SetDatabasesToSearch(const TArray<UPoseSearchDatabase*>& InDatabases, bool bForceInterruptIfNew)
{
	// Check if InDatabases and DatabasesToSearch are the same.
	bool bDatabasesAlreadySet = true;
	if (DatabasesToSearch.Num() != InDatabases.Num())
	{
		bDatabasesAlreadySet = false;
	}
	else
	{
		for (int32 Index = 0; Index < InDatabases.Num(); ++Index)
		{
			if (DatabasesToSearch[Index] != InDatabases[Index])
			{
				bDatabasesAlreadySet = false;
				break;
			}
		}
	}

	if (bDatabasesAlreadySet)
	{
		UE_LOG(LogPoseSearch, Verbose, TEXT("FAnimNode_MotionMatching::SetDatabasesToSearch - Databases(#%d) already set."), InDatabases.Num());
	}
	else
	{
		DatabasesToSearch.Reset();
		bOverrideDatabaseInput = false;
		if (!InDatabases.IsEmpty())
		{
			DatabasesToSearch.Append(InDatabases);
			bOverrideDatabaseInput = true;
		}

		bForceInterruptNextUpdate |= bForceInterruptIfNew;

		UE_LOG(LogPoseSearch, Verbose, TEXT("FAnimNode_MotionMatching::SetDatabaseToSearch - Setting to Databases(#%d), bForceInterruptIfNew(%d)."), InDatabases.Num(), bForceInterruptIfNew);
	}
}

void FAnimNode_MotionMatching::ResetDatabasesToSearch(bool bInForceInterrupt)
{
	DatabasesToSearch.Reset();
	bOverrideDatabaseInput = false;
	bForceInterruptNextUpdate = bInForceInterrupt;

	UE_LOG(LogPoseSearch, Verbose, TEXT("FAnimNode_MotionMatching::ResetDatabasesToSearch - Resetting databases, bInForceInterrupt(%d)."), bInForceInterrupt);
}

void FAnimNode_MotionMatching::ForceInterruptNextUpdate()
{
	bForceInterruptNextUpdate = true;

	UE_LOG(LogPoseSearch, Verbose, TEXT("FAnimNode_MotionMatching::ForceInterruptNextUpdate - Forcing interrupt."));
}

// FAnimNode_AssetPlayerBase interface
float FAnimNode_MotionMatching::GetAccumulatedTime() const
{
	return BlendStackNode.GetAccumulatedTime();
}

UAnimationAsset* FAnimNode_MotionMatching::GetAnimAsset() const
{
	return BlendStackNode.GetAnimAsset();
}

float FAnimNode_MotionMatching::GetCurrentAssetLength() const
{
	return BlendStackNode.GetCurrentAssetLength();
}

float FAnimNode_MotionMatching::GetCurrentAssetTime() const
{
	return BlendStackNode.GetCurrentAssetLength();
}

float FAnimNode_MotionMatching::GetCurrentAssetTimePlayRateAdjusted() const
{
	return BlendStackNode.GetCurrentAssetTimePlayRateAdjusted();
}

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
