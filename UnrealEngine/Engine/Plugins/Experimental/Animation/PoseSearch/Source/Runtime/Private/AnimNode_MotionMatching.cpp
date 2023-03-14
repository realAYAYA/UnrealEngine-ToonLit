// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/AnimNode_MotionMatching.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/MotionTrajectoryTypes.h"
#include "Animation/AnimRootMotionProvider.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "Trace/PoseSearchTraceLogger.h"

#define LOCTEXT_NAMESPACE "AnimNode_MotionMatching"

/////////////////////////////////////////////////////
// FAnimNode_MotionMatching

void FAnimNode_MotionMatching::Initialize_AnyThread(const FAnimationInitializeContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Initialize_AnyThread);

	GetEvaluateGraphExposedInputs().Execute(Context);

	BlendStackNode.Initialize_AnyThread(Context);

	Source.SetLinkNode(&BlendStackNode);
	Source.Initialize(Context);
}

void FAnimNode_MotionMatching::Evaluate_AnyThread(FPoseContext& Output)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(Evaluate_AnyThread);

	Source.Evaluate(Output);

#if WITH_EDITORONLY_DATA
	bWasEvaluated = true;
#endif

#if UE_POSE_SEARCH_TRACE_ENABLED
	MotionMatchingState.RootMotionTransformDelta = FTransform::Identity;

	const UE::Anim::IAnimRootMotionProvider* RootMotionProvider = UE::Anim::IAnimRootMotionProvider::Get();

	if (ensureMsgf(RootMotionProvider, TEXT("Could not get Root Motion Provider.")))
	{
		if (RootMotionProvider->HasRootMotion(Output.CustomAttributes))
		{
			RootMotionProvider->ExtractRootMotion(Output.CustomAttributes, MotionMatchingState.RootMotionTransformDelta);
		}
	}
#endif
}

void FAnimNode_MotionMatching::UpdateAssetPlayer(const FAnimationUpdateContext& Context)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_ANIMNODE(UpdateAssetPlayer);

	GetEvaluateGraphExposedInputs().Execute(Context);

	bool bNeedsReset =
		bResetOnBecomingRelevant &&
		UpdateCounter.HasEverBeenUpdated() &&
		!UpdateCounter.WasSynchronizedCounter(Context.AnimInstanceProxy->GetUpdateCounter());

	// If we just became relevant and haven't been initialized yet, then reset motion matching state, otherwise update the asset time using the player node.
	if (bNeedsReset)
	{
		MotionMatchingState.Reset();
	}
	else
	{
		// We adjust the motion matching state asset time to the current player node's asset time. This is done 
		// because the player node may have ticked more or less time than we expected due to variable dt or the 
		// dynamic playback rate adjustment and as such the motion matching state does not update by itself
		MotionMatchingState.AdjustAssetTime(BlendStackNode.GetAccumulatedTime());
	}
	UpdateCounter.SynchronizeWith(Context.AnimInstanceProxy->GetUpdateCounter());

	// Execute core motion matching algorithm
	UpdateMotionMatchingState(
		Context,
		Searchable,
		&ActiveTagsContainer,
		Trajectory,
		Settings,
		MotionMatchingState,
		bForceInterrupt
	);

	// If a new pose is requested, blend into the new asset via BlendStackNode
	const UPoseSearchDatabase* Database = MotionMatchingState.CurrentSearchResult.Database.Get();
	const FPoseSearchIndexAsset* SearchIndexAsset = MotionMatchingState.GetCurrentSearchIndexAsset();

	if (Database && Database->Schema && SearchIndexAsset && (MotionMatchingState.Flags & EMotionMatchingFlags::JumpedToPose) == EMotionMatchingFlags::JumpedToPose)
	{
		const FPoseSearchDatabaseAnimationAssetBase& AnimationAssetBase = Database->GetAnimationSourceAsset(SearchIndexAsset);
		BlendStackNode.BlendTo(	SearchIndexAsset->Type, AnimationAssetBase.GetAnimationAsset(), MotionMatchingState.CurrentSearchResult.AssetTime,
								AnimationAssetBase.IsLooping(), SearchIndexAsset->bMirrored, Database->Schema->MirrorDataTable.Get(),
								Settings.MaxActiveBlends, Settings.BlendTime, Settings.BlendProfile, Settings.BlendOption, SearchIndexAsset->BlendParameters);
	}

	Source.Update(Context);
}

bool FAnimNode_MotionMatching::HasPreUpdate() const
{
#if WITH_EDITORONLY_DATA
	return true;
#else
	return false;
#endif
}

void FAnimNode_MotionMatching::PreUpdate(const UAnimInstance* InAnimInstance)
{
#if WITH_EDITORONLY_DATA
	using namespace UE::PoseSearch;

	if (bWasEvaluated && bDebugDraw)
	{
		USkeletalMeshComponent* SkeletalMeshComponent = InAnimInstance->GetSkelMeshComponent();
		check(SkeletalMeshComponent);

		const UE::PoseSearch::FSearchResult& CurResult = MotionMatchingState.CurrentSearchResult;
		UE::PoseSearch::FDebugDrawParams DrawParams;
		DrawParams.RootTransform = SkeletalMeshComponent->GetComponentTransform();
		DrawParams.Database = CurResult.Database.Get();
		DrawParams.World = SkeletalMeshComponent->GetWorld();
		DrawParams.DefaultLifeTime = 0.0f;

		if (DrawParams.CanDraw())
		{
			if (bDebugDrawMatch)
			{
				DrawFeatureVector(DrawParams, CurResult.PoseIdx);
			}

			if (bDebugDrawQuery)
			{
				EnumAddFlags(DrawParams.Flags, EDebugDrawFlags::DrawQuery);
				DrawFeatureVector(DrawParams, CurResult.ComposedQuery.GetValues());
			}

			if (DrawParams.Database->PoseSearchMode == EPoseSearchMode::PCAKDTree_Compare)
			{
				FDebugFloatHistory& C = MotionMatchingState.SearchCostHistoryContinuing;
				FDebugFloatHistory& B = MotionMatchingState.SearchCostHistoryBruteForce;
				FDebugFloatHistory& K = MotionMatchingState.SearchCostHistoryKDTree;

				C.AddSample(CurResult.ContinuingPoseCost.IsValid() ? CurResult.ContinuingPoseCost.GetTotalCost() : C.MaxValue);
				B.AddSample(CurResult.BruteForcePoseCost.IsValid() ? CurResult.BruteForcePoseCost.GetTotalCost() : B.MaxValue);
				K.AddSample(CurResult.PoseCost.IsValid() ? CurResult.PoseCost.GetTotalCost() : K.MaxValue);

				// making SearchCostHistoryKDTree and SearchCostHistoryBruteForce min max consistent
				const float MinValue = FMath::Min(C.MinValue, FMath::Min(B.MinValue, K.MinValue));
				const float MaxValue = FMath::Max(C.MaxValue, FMath::Max(B.MaxValue, K.MaxValue));
				
				C.MinValue = MinValue;
				C.MaxValue = MaxValue;

				B.MinValue = MinValue;
				B.MaxValue = MaxValue;

				K.MinValue = MinValue;
				K.MaxValue = MaxValue;

				const FVector2D DrawSize(150.f, 100.f);
				const FTransform OffsetTransform(FRotator(0.f, 0.f, 0.f), FVector(-50.f, -75.f, 100.f));
				const FTransform DrawTransform = OffsetTransform * DrawParams.RootTransform;

				DrawDebugFloatHistory(*DrawParams.World, K, OffsetTransform * DrawParams.RootTransform, DrawSize, FColor(255, 192, 203, 160)); // pink
				DrawDebugFloatHistory(*DrawParams.World, B, OffsetTransform * DrawParams.RootTransform, DrawSize, FColor(0, 0, 255, 160)); // blue
				DrawDebugFloatHistory(*DrawParams.World, C, OffsetTransform * DrawParams.RootTransform, DrawSize, FColor(160, 160, 160, 160)); // gray
			}
		}
	}

	bWasEvaluated = false;
#endif
}

void FAnimNode_MotionMatching::GatherDebugData(FNodeDebugData& DebugData)
{
	Source.GatherDebugData(DebugData);
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