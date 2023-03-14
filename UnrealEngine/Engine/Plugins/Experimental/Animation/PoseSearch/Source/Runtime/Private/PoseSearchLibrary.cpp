// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchLibrary.h"

#include "Animation/AnimSequence.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/BlendSpace.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "Trace/PoseSearchTraceLogger.h"
#include "MotionTrajectoryLibrary.h"

#define LOCTEXT_NAMESPACE "PoseSearchLibrary"

//////////////////////////////////////////////////////////////////////////
// FMotionMatchingState

void FMotionMatchingState::Reset()
{
	CurrentSearchResult.Reset();
	// Set the elapsed time to INFINITY to trigger a search right away
	ElapsedPoseJumpTime = INFINITY;
	PoseIndicesHistory.Reset();
}

void FMotionMatchingState::AdjustAssetTime(float AssetTime)
{
	CurrentSearchResult.Update(AssetTime);
}

bool FMotionMatchingState::CanAdvance(float DeltaTime, bool& bOutAdvanceToFollowUpAsset, UE::PoseSearch::FSearchResult& OutFollowUpAsset) const
{
	bOutAdvanceToFollowUpAsset = false;
	OutFollowUpAsset = UE::PoseSearch::FSearchResult();

	if (!CurrentSearchResult.IsValid())
	{
		return false;
	}

	const FPoseSearchIndexAsset* SearchIndexAsset = GetCurrentSearchIndexAsset();

	if (SearchIndexAsset->Type == ESearchIndexAssetType::Sequence)
	{
		const FPoseSearchDatabaseSequence& DbSequence = 
			CurrentSearchResult.Database->GetSequenceSourceAsset(SearchIndexAsset);
		const float AssetLength = DbSequence.Sequence->GetPlayLength();

		float SteppedTime = CurrentSearchResult.AssetTime;
		ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(
			DbSequence.Sequence->bLoop,
			DeltaTime,
			SteppedTime,
			AssetLength);

		if (AdvanceType != ETAA_Finished)
		{
			return SearchIndexAsset->SamplingInterval.Contains(SteppedTime);
		}
		else
		{
			// check if there's a follow-up that can be used
			int32 FollowUpDbSequenceIdx = CurrentSearchResult.Database->Sequences.IndexOfByPredicate(
				[&](const FPoseSearchDatabaseSequence& Entry)
				{
					return Entry.Sequence == DbSequence.FollowUpSequence;
				});

			int32 FollowUpSearchIndexAssetIdx = CurrentSearchResult.Database->GetSearchIndex()->Assets.IndexOfByPredicate(
				[&](const FPoseSearchIndexAsset& Entry)
				{
					const bool bIsMatch =
						Entry.SourceAssetIdx == FollowUpDbSequenceIdx &&
						Entry.bMirrored == SearchIndexAsset->bMirrored &&
						Entry.SamplingInterval.Contains(0.0f);
					return bIsMatch;
				});

			if (FollowUpSearchIndexAssetIdx != INDEX_NONE)
			{
				bOutAdvanceToFollowUpAsset = true;

				const FPoseSearchIndexAsset* FollowUpSearchIndexAsset =
					&CurrentSearchResult.Database->GetSearchIndex()->Assets[FollowUpSearchIndexAssetIdx];

				// Follow up asset time will start slightly before the beginning of the sequence as 
				// this is essentially what the matching time in the corresponding main sequence is.
				// Here we are assuming that the tick will advance the asset player timer into the 
				// valid region
				const float FollowUpAssetTime = CurrentSearchResult.AssetTime - AssetLength;

				// There is no correspoding pose index when we switch due to what is mentioned above
				// so for now we just take whatever pose index is associated with the first frame.
				OutFollowUpAsset.PoseIdx = CurrentSearchResult.Database->GetPoseIndexFromTime(FollowUpSearchIndexAsset->SamplingInterval.Min, FollowUpSearchIndexAsset);
				OutFollowUpAsset.SearchIndexAsset = FollowUpSearchIndexAsset;
				OutFollowUpAsset.AssetTime = FollowUpAssetTime;
				return true;
			}
		}
	}
	else if (SearchIndexAsset->Type == ESearchIndexAssetType::BlendSpace)
	{
		const FPoseSearchDatabaseBlendSpace& DbBlendSpace = 
			CurrentSearchResult.Database->GetBlendSpaceSourceAsset(SearchIndexAsset);

		TArray<FBlendSampleData> BlendSamples;
		int32 TriangulationIndex = 0;
		DbBlendSpace.BlendSpace->GetSamplesFromBlendInput(SearchIndexAsset->BlendParameters, BlendSamples, TriangulationIndex, true);

		float PlayLength = DbBlendSpace.BlendSpace->GetAnimationLengthFromSampleData(BlendSamples);

		// Asset player time for blendspaces is normalized [0, 1] so we need to convert 
		// to a real time before we advance it
		float RealTime = CurrentSearchResult.AssetTime * PlayLength;
		float SteppedTime = RealTime;
		ETypeAdvanceAnim AdvanceType = FAnimationRuntime::AdvanceTime(
			DbBlendSpace.BlendSpace->bLoop,
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
	// Remember which pose and sequence we're playing from the database
	CurrentSearchResult = Result;

	ElapsedPoseJumpTime = 0.0f;
	
	// requesting inertial blending only if blendstack is disabled
	if (Settings.MaxActiveBlends <= 0)
	{
		const float JumpBlendTime = ComputeJumpBlendTime(Result, Settings);
		RequestInertialBlend(Context, JumpBlendTime);
	}

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

	if (!MotionMatchingState.CurrentSearchResult.IsValid())
	{
		return;
	}

	const float DeltaTime = UpdateContext.GetDeltaTime();

	auto AddUniqueDatabase = [](TArray<FTraceMotionMatchingStateDatabaseEntry>& DatabaseEntries, const UPoseSearchDatabase* Database, const UE::PoseSearch::FSearchContext& SearchContext) -> int32
	{
		const uint64 DatabaseId = FTraceMotionMatchingState::GetIdFromObject(Database);

		int32 DbEntryIdx = -1;
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

			const FPoseSearchFeatureVectorBuilder* CachedQuery = SearchContext.GetCachedQuery(Database);
			check(CachedQuery);

			DatabaseEntries[DbEntryIdx].QueryVector = CachedQuery->GetValues();
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
		int32 FirstIdx = 0;
		const FTrajectorySample PrevSample = FTrajectorySampleRange::IterSampleTrajectory(
			Trajectory.Samples,
			ETrajectorySampleDomain::Time,
			-DeltaTime, FirstIdx);

		const FTrajectorySample CurrSample = FTrajectorySampleRange::IterSampleTrajectory(
			Trajectory.Samples,
			ETrajectorySampleDomain::Time,
			0.0f, FirstIdx);

		const FTransform SimDelta = CurrSample.Transform.GetRelativeTransform(PrevSample.Transform);

		TraceState.SimLinearVelocity = SimDelta.GetTranslation().Size() / DeltaTime;
		TraceState.SimAngularVelocity = FMath::RadiansToDegrees(SimDelta.GetRotation().GetAngle()) / DeltaTime;

		// animation
		const FTransform AnimDelta = MotionMatchingState.RootMotionTransformDelta;

		TraceState.AnimLinearVelocity = AnimDelta.GetTranslation().Size() / DeltaTime;
		TraceState.AnimAngularVelocity = FMath::RadiansToDegrees(AnimDelta.GetRotation().GetAngle()) / DeltaTime;
	}

	if (EnumHasAnyFlags(MotionMatchingState.Flags, EMotionMatchingFlags::JumpedToFollowUp))
	{
		TraceState.Flags |= FTraceMotionMatchingState::EFlags::FollowupAnimation;
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

	// Check if we can advance. Includes the case where we can advance but only by switching to a follow up asset.
	bool bAdvanceToFollowUpAsset = false;
	FSearchResult FollowUpAssetResult;
	const bool bCanAdvance = InOutMotionMatchingState.CanAdvance(DeltaTime, bAdvanceToFollowUpAsset, FollowUpAssetResult);

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

		if (const FPoseSearchIndexAsset* CurrentIndexAsset = InOutMotionMatchingState.GetCurrentSearchIndexAsset())
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

	// If we didn't search or it didn't find a pose to jump to, and we can 
	// advance but only with the follow up asset, jump to that. Otherwise we 
	// are advancing as normal, and nothing needs to be done.
	if (!(InOutMotionMatchingState.Flags & EMotionMatchingFlags::JumpedToPose)
		&& bCanAdvance
		&& bAdvanceToFollowUpAsset)
	{
		InOutMotionMatchingState.JumpToPose(Context, Settings, FollowUpAssetResult);
		InOutMotionMatchingState.Flags |= EMotionMatchingFlags::JumpedToFollowUp;
	}

	// Tick elapsed pose jump timer
	if (!(InOutMotionMatchingState.Flags & EMotionMatchingFlags::JumpedToPose))
	{
		InOutMotionMatchingState.ElapsedPoseJumpTime += DeltaTime;
	}

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

const FPoseSearchIndexAsset* FMotionMatchingState::GetCurrentSearchIndexAsset() const
{
	if (CurrentSearchResult.IsValid())
	{
		return CurrentSearchResult.SearchIndexAsset;
	}

	return nullptr;
}

float FMotionMatchingState::ComputeJumpBlendTime(
	const UE::PoseSearch::FSearchResult& Result, 
	const FMotionMatchingSettings& Settings
) const
{
	const FPoseSearchIndexAsset* SearchIndexAsset = GetCurrentSearchIndexAsset();

	// Use alternate blend time when changing between mirrored and unmirrored
	float JumpBlendTime = Settings.BlendTime;
	if ((SearchIndexAsset != nullptr) && (Settings.MirrorChangeBlendTime > 0.0f))
	{
		if (Result.SearchIndexAsset->bMirrored != SearchIndexAsset->bMirrored)
		{
			JumpBlendTime = Settings.MirrorChangeBlendTime;
		}
	}

	return JumpBlendTime;
}

#undef LOCTEXT_NAMESPACE