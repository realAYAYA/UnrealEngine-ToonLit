// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchLibrary.h"

#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/BlendSpace.h"
#include "InstancedStruct.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchFeatureChannel_Trajectory.h"
#include "Trace/PoseSearchTraceLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoseSearchLibrary)

#define LOCTEXT_NAMESPACE "PoseSearchLibrary"

//////////////////////////////////////////////////////////////////////////
// FMotionMatchingState

void FMotionMatchingState::Reset()
{
	CurrentSearchResult.Reset();
	// Set the elapsed time to INFINITY to trigger a search right away
	ElapsedPoseJumpTime = INFINITY;
	PoseIndicesHistory.Reset();
	WantedPlayRate = 1.f;
}

void FMotionMatchingState::AdjustAssetTime(float AssetTime)
{
	CurrentSearchResult.Update(AssetTime);
}

bool FMotionMatchingState::CanAdvance(float DeltaTime) const
{
	if (!CurrentSearchResult.IsValid())
	{
		return false;
	}

	const FPoseSearchIndexAsset* SearchIndexAsset = CurrentSearchResult.GetSearchIndexAsset(true);

	const FInstancedStruct& DatabaseAsset = CurrentSearchResult.Database->GetAnimationAssetStruct(*SearchIndexAsset);
	if (const FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAsset.GetPtr<FPoseSearchDatabaseSequence>())
	{
		const float AssetLength = DatabaseSequence->Sequence->GetPlayLength();

		float SteppedTime = CurrentSearchResult.AssetTime;
		ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(
			DatabaseSequence->Sequence->bLoop,
			DeltaTime,
			SteppedTime,
			AssetLength);

		if (AdvanceType != ETAA_Finished)
		{
			return SearchIndexAsset->SamplingInterval.Contains(SteppedTime);
		}
	}
	else if (const FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAsset.GetPtr<FPoseSearchDatabaseAnimComposite>())
	{
		const float AssetLength = DatabaseAnimComposite->GetAnimationAsset()->GetPlayLength();

		float SteppedTime = CurrentSearchResult.AssetTime;
		ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(
			DatabaseAnimComposite->IsLooping(),
			DeltaTime,
			SteppedTime,
			AssetLength);

		if (AdvanceType != ETAA_Finished)
		{
			return SearchIndexAsset->SamplingInterval.Contains(SteppedTime);
		}
	}
	else if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAsset.GetPtr<FPoseSearchDatabaseBlendSpace>())
	{
		TArray<FBlendSampleData> BlendSamples;
		int32 TriangulationIndex = 0;
		DatabaseBlendSpace->BlendSpace->GetSamplesFromBlendInput(SearchIndexAsset->BlendParameters, BlendSamples, TriangulationIndex, true);

		float PlayLength = DatabaseBlendSpace->BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);

		// Asset player time for blendspaces is normalized [0, 1] so we need to convert 
		// to a real time before we advance it
		float RealTime = CurrentSearchResult.AssetTime * PlayLength;
		float SteppedTime = RealTime;
		ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(
			DatabaseBlendSpace->BlendSpace->bLoop,
			DeltaTime,
			SteppedTime,
			PlayLength);

		if (AdvanceType != ETAA_Finished)
		{
			return SearchIndexAsset->SamplingInterval.Contains(SteppedTime);
		}
	}
	else
	{
		checkNoEntry();
	}

	return false;
}

static void RequestInertialBlend(const FAnimationUpdateContext& Context, float BlendTime)
{
	// Use inertial blending to smooth over the transition
	// It would be cool in the future to adjust the blend time by amount of dissimilarity, but we'll need a standardized distance metric first.
	if (BlendTime > 0.0f)
	{
		UE::Anim::IInertializationRequester* InertializationRequester = Context.GetMessage<UE::Anim::IInertializationRequester>();
		if (InertializationRequester)
		{
			InertializationRequester->RequestInertialization(BlendTime);
		}
	}
}

void FMotionMatchingState::JumpToPose(const FAnimationUpdateContext& Context, const FMotionMatchingSettings& Settings, const UE::PoseSearch::FSearchResult& Result)
{
	// requesting inertial blending only if blendstack is disabled
	if (Settings.MaxActiveBlends <= 0)
	{
		const float JumpBlendTime = ComputeJumpBlendTime(Result, Settings);
		RequestInertialBlend(Context, JumpBlendTime);
	}

	// Remember which pose and sequence we're playing from the database
	CurrentSearchResult = Result;

	ElapsedPoseJumpTime = 0.0f;

	Flags |= EMotionMatchingFlags::JumpedToPose;
}

#if UE_POSE_SEARCH_TRACE_ENABLED
static void TraceMotionMatchingState(
	const FAnimationUpdateContext& UpdateContext,
	const UPoseSearchSearchableAsset* Searchable,
	UE::PoseSearch::FSearchContext& SearchContext,
	const FMotionMatchingState& MotionMatchingState,
	const FTrajectorySampleRange& Trajectory,
	const UE::PoseSearch::FSearchResult& LastResult)
{
	using namespace UE::PoseSearch;

	if (!IsTracing(UpdateContext))
	{
		return;
	}

	const float DeltaTime = UpdateContext.GetDeltaTime();

	auto AddUniqueDatabase = [](TArray<FTraceMotionMatchingStateDatabaseEntry>& DatabaseEntries, const UPoseSearchDatabase* Database, UE::PoseSearch::FSearchContext& SearchContext) -> int32
	{
		const uint64 DatabaseId = FTraceMotionMatchingState::GetIdFromObject(Database);

		int32 DbEntryIdx = INDEX_NONE;
		for (int32 i = 0; i < DatabaseEntries.Num(); ++i)
		{
			if (DatabaseEntries[i].DatabaseId == DatabaseId)
			{
				DbEntryIdx = i;
				break;
			}
		}
		if (DbEntryIdx == -1)
		{
			DbEntryIdx = DatabaseEntries.Add({ DatabaseId });

			// if throttling is on, the continuing pose can be valid, but no actual search occurred, so the query will not be cached, and we need to build it
			FPoseSearchFeatureVectorBuilder FeatureVectorBuilder;
			FeatureVectorBuilder.Init(Database->Schema);
			SearchContext.GetOrBuildQuery(Database, FeatureVectorBuilder);
			DatabaseEntries[DbEntryIdx].QueryVector = FeatureVectorBuilder.GetValues();
		}

		return DbEntryIdx;
	};

	FTraceMotionMatchingState TraceState;
	while (!SearchContext.BestCandidates.IsEmpty())
	{
		FSearchContext::FPoseCandidate PoseCandidate;
		SearchContext.BestCandidates.Pop(PoseCandidate);

		const int32 DbEntryIdx = AddUniqueDatabase(TraceState.DatabaseEntries, PoseCandidate.Database, SearchContext);
		FTraceMotionMatchingStateDatabaseEntry& DbEntry = TraceState.DatabaseEntries[DbEntryIdx];

		FTraceMotionMatchingStatePoseEntry PoseEntry;
		PoseEntry.DbPoseIdx = PoseCandidate.PoseIdx;
		PoseEntry.Cost = PoseCandidate.Cost;
		PoseEntry.PoseCandidateFlags = PoseCandidate.PoseCandidateFlags;

		DbEntry.PoseEntries.Add(PoseEntry);
	}

	if (MotionMatchingState.CurrentSearchResult.ContinuingPoseCost.IsValid())
	{
		check(LastResult.IsValid());

		const int32 DbEntryIdx = AddUniqueDatabase(TraceState.DatabaseEntries, LastResult.Database.Get(), SearchContext);
		FTraceMotionMatchingStateDatabaseEntry& DbEntry = TraceState.DatabaseEntries[DbEntryIdx];

		const int32 LastResultPoseEntryIdx = DbEntry.PoseEntries.Add({LastResult.PoseIdx});
		FTraceMotionMatchingStatePoseEntry& PoseEntry = DbEntry.PoseEntries[LastResultPoseEntryIdx];

		PoseEntry.Cost = MotionMatchingState.CurrentSearchResult.ContinuingPoseCost;
		PoseEntry.PoseCandidateFlags = EPoseCandidateFlags::Valid_ContinuingPose;
	}

	if (MotionMatchingState.CurrentSearchResult.PoseCost.IsValid())
	{
		const int32 DbEntryIdx = AddUniqueDatabase(TraceState.DatabaseEntries, MotionMatchingState.CurrentSearchResult.Database.Get(), SearchContext);
		FTraceMotionMatchingStateDatabaseEntry& DbEntry = TraceState.DatabaseEntries[DbEntryIdx];

		const int32 PoseEntryIdx = DbEntry.PoseEntries.Add({MotionMatchingState.CurrentSearchResult.PoseIdx});
		FTraceMotionMatchingStatePoseEntry& PoseEntry = DbEntry.PoseEntries[PoseEntryIdx];

		PoseEntry.Cost = MotionMatchingState.CurrentSearchResult.PoseCost;
		PoseEntry.PoseCandidateFlags = EPoseCandidateFlags::Valid_CurrentPose;

		TraceState.CurrentDbEntryIdx = DbEntryIdx;
		TraceState.CurrentPoseEntryIdx = PoseEntryIdx;
	}

	if (DeltaTime > SMALL_NUMBER)
	{
		// simulation
		const FTrajectorySample PrevSample = Trajectory.GetSampleAtTime(-DeltaTime);
		const FTrajectorySample CurrSample = Trajectory.GetSampleAtTime(0.f);

		const FTransform SimDelta = CurrSample.Transform.GetRelativeTransform(PrevSample.Transform);

		TraceState.SimLinearVelocity = SimDelta.GetTranslation().Size() / DeltaTime;
		TraceState.SimAngularVelocity = FMath::RadiansToDegrees(SimDelta.GetRotation().GetAngle()) / DeltaTime;

		// animation
		const FTransform AnimDelta = MotionMatchingState.RootMotionTransformDelta;

		TraceState.AnimLinearVelocity = AnimDelta.GetTranslation().Size() / DeltaTime;
		TraceState.AnimAngularVelocity = FMath::RadiansToDegrees(AnimDelta.GetRotation().GetAngle()) / DeltaTime;
	}

	TraceState.SearchableAssetId = FTraceMotionMatchingState::GetIdFromObject(Searchable);
	TraceState.ElapsedPoseJumpTime = MotionMatchingState.ElapsedPoseJumpTime;
	TraceState.AssetPlayerTime = MotionMatchingState.CurrentSearchResult.AssetTime;
	TraceState.DeltaTime = DeltaTime;

	TraceState.Output(UpdateContext);
}
#endif

void UpdateMotionMatchingState(
	const FAnimationUpdateContext& Context,
	const UPoseSearchSearchableAsset* Searchable,
	const FGameplayTagContainer* ActiveTagsContainer,
	const FTrajectorySampleRange& Trajectory,
	const FMotionMatchingSettings& Settings,
	FMotionMatchingState& InOutMotionMatchingState,
	bool bForceInterrupt
)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_Update);

	using namespace UE::PoseSearch;

	if (!Searchable)
	{
		Context.LogMessage(
			EMessageSeverity::Error, 
			LOCTEXT("NoSearchable", "No searchable asset provided for motion matching."));
		return;
	}

	const float DeltaTime = Context.GetDeltaTime();

	// Reset State Flags
	InOutMotionMatchingState.Flags = EMotionMatchingFlags::None;

#if UE_POSE_SEARCH_TRACE_ENABLED
	// Record Current Pose Index for Debugger
	const FSearchResult LastResult = InOutMotionMatchingState.CurrentSearchResult;
#endif

	// Check if we can advance.
	const bool bCanAdvance = InOutMotionMatchingState.CanAdvance(DeltaTime);

	// If we can't advance or enough time has elapsed since the last pose jump then search
	FSearchContext SearchContext;
	if (!bCanAdvance || (InOutMotionMatchingState.ElapsedPoseJumpTime >= Settings.SearchThrottleTime))
	{
		// Build the search context
		SearchContext.ActiveTagsContainer = ActiveTagsContainer;
		SearchContext.Trajectory = &Trajectory;
		SearchContext.OwningComponent = Context.AnimInstanceProxy->GetSkelMeshComponent();
		SearchContext.BoneContainer = &Context.AnimInstanceProxy->GetRequiredBones();
		SearchContext.bIsTracing = IsTracing(Context);
		SearchContext.bForceInterrupt = bForceInterrupt;
		SearchContext.bCanAdvance = bCanAdvance;
		SearchContext.CurrentResult = InOutMotionMatchingState.CurrentSearchResult;
		SearchContext.PoseJumpThresholdTime = Settings.PoseJumpThresholdTime;
		SearchContext.PoseIndicesHistory = &InOutMotionMatchingState.PoseIndicesHistory;

		IPoseHistoryProvider* PoseHistoryProvider = Context.GetMessage<IPoseHistoryProvider>();
		if (PoseHistoryProvider)
		{
			SearchContext.History = &PoseHistoryProvider->GetPoseHistory();
		}

		if (const FPoseSearchIndexAsset* CurrentIndexAsset = InOutMotionMatchingState.CurrentSearchResult.GetSearchIndexAsset())
		{
			SearchContext.QueryMirrorRequest =
				CurrentIndexAsset->bMirrored ?
				EPoseSearchBooleanRequest::TrueValue :
				EPoseSearchBooleanRequest::FalseValue;
		}

		// Search the database for the nearest match to the updated query vector
		FSearchResult SearchResult = Searchable->Search(SearchContext);

		// making sure we haven't calculated ContinuingPoseCost if we !bCanAdvance 
		check(bCanAdvance || !SearchResult.ContinuingPoseCost.IsValid());
		
		if (SearchResult.PoseCost.GetTotalCost() < SearchResult.ContinuingPoseCost.GetTotalCost())
		{
			InOutMotionMatchingState.JumpToPose(Context, Settings, SearchResult);
		}
		else
		{
			// copying few properties of SearchResult into CurrentSearchResult to facilitate debug drawing
#if WITH_EDITOR
			InOutMotionMatchingState.CurrentSearchResult.BruteForcePoseCost = SearchResult.BruteForcePoseCost;
#endif
			InOutMotionMatchingState.CurrentSearchResult.PoseCost = SearchResult.PoseCost;
			InOutMotionMatchingState.CurrentSearchResult.ContinuingPoseCost = SearchResult.ContinuingPoseCost;
			InOutMotionMatchingState.CurrentSearchResult.ComposedQuery = SearchResult.ComposedQuery;
		}
	}

	// Tick elapsed pose jump timer
	if (!(InOutMotionMatchingState.Flags & EMotionMatchingFlags::JumpedToPose))
	{
		InOutMotionMatchingState.ElapsedPoseJumpTime += DeltaTime;
	}

	InOutMotionMatchingState.UpdateWantedPlayRate(SearchContext, Settings);
	InOutMotionMatchingState.PoseIndicesHistory.Update(InOutMotionMatchingState.CurrentSearchResult, DeltaTime, Settings.PoseReselectHistory);

#if UE_POSE_SEARCH_TRACE_ENABLED
	// Record debugger details
	TraceMotionMatchingState(
		Context,
		Searchable,
		SearchContext,
		InOutMotionMatchingState,
		Trajectory,
		LastResult);
#endif
}

void FMotionMatchingState::UpdateWantedPlayRate(const UE::PoseSearch::FSearchContext& SearchContext, const FMotionMatchingSettings& Settings)
{
	if (CurrentSearchResult.IsValid())
	{
		if (!FMath::IsNearlyEqual(Settings.PlayRateMin, 1.f, UE_KINDA_SMALL_NUMBER) || !FMath::IsNearlyEqual(Settings.PlayRateMax, 1.f, UE_KINDA_SMALL_NUMBER))
		{
			if (const FPoseSearchFeatureVectorBuilder* PoseSearchFeatureVectorBuilder = SearchContext.GetCachedQuery(CurrentSearchResult.Database.Get()))
			{
				if (const UPoseSearchFeatureChannel_Trajectory* TrajectoryChannel = CurrentSearchResult.Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Trajectory>())
				{
					TConstArrayView<float> QueryData = PoseSearchFeatureVectorBuilder->GetValues();
					TConstArrayView<float> ResultData = CurrentSearchResult.Database->GetSearchIndex().GetPoseValues(CurrentSearchResult.PoseIdx);
					const float EstimatedSpeedRatio = TrajectoryChannel->GetEstimatedSpeedRatio(QueryData, ResultData);
					check(Settings.PlayRateMin <= Settings.PlayRateMax);
					WantedPlayRate = FMath::Clamp(EstimatedSpeedRatio, Settings.PlayRateMin, Settings.PlayRateMax);
				}
				else
				{
					UE_LOG(LogPoseSearch, Warning,
						TEXT("Couldn't update the WantedPlayRate in FMotionMatchingState::UpdateWantedPlayRate, because Schema '%s' couldn't find a UPoseSearchFeatureChannel_Trajectory channel"),
						*GetNameSafe(CurrentSearchResult.Database->Schema));
				}
			}
		}
	}
}

float FMotionMatchingState::ComputeJumpBlendTime(const UE::PoseSearch::FSearchResult& Result, const FMotionMatchingSettings& Settings) const
{
	const FPoseSearchIndexAsset* SearchIndexAsset = CurrentSearchResult.GetSearchIndexAsset();

	// Use alternate blend time when changing between mirrored and unmirrored
	float JumpBlendTime = Settings.BlendTime;
	if (SearchIndexAsset && Settings.MirrorChangeBlendTime > 0.0f)
	{
		if (Result.GetSearchIndexAsset(true)->bMirrored != SearchIndexAsset->bMirrored)
		{
			JumpBlendTime = Settings.MirrorChangeBlendTime;
		}
	}

	return JumpBlendTime;
}

#undef LOCTEXT_NAMESPACE
