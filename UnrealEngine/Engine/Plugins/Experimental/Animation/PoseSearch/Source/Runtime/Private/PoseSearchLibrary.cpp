// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearch/PoseSearchLibrary.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimNode_Inertialization.h"
#include "Animation/AnimNode_SequencePlayer.h"
#include "Animation/AnimRootMotionProvider.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSubsystem_Tag.h"
#include "Animation/BlendSpace.h"
#include "Animation/BuiltInAttributeTypes.h"
#include "Animation/AnimTrace.h"
#include "InstancedStruct.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchFeatureChannel_Trajectory.h"
#include "Trace/PoseSearchTraceLogger.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoseSearchLibrary)

#define LOCTEXT_NAMESPACE "PoseSearchLibrary"

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
TAutoConsoleVariable<int32> CVarAnimMotionMatchDrawQueryEnable(TEXT("a.MotionMatch.DrawQuery.Enable"), 0, TEXT("Enable / Disable MotionMatch Draw Query"));
TAutoConsoleVariable<int32> CVarAnimMotionMatchDrawMatchEnable(TEXT("a.MotionMatch.DrawMatch.Enable"), 0, TEXT("Enable / Disable MotionMatch Draw Match"));
TAutoConsoleVariable<int32> CVarAnimMotionMatchDrawHistoryEnable(TEXT("a.MotionMatch.DrawHistory.Enable"), 0, TEXT("Enable / Disable MotionMatch Draw History"));
#endif

//////////////////////////////////////////////////////////////////////////
// FMotionMatchingState

void FMotionMatchingState::Reset(const FTransform& ComponentTransform)
{
	CurrentSearchResult.Reset();
	// Set the elapsed time to INFINITY to trigger a search right away
	ElapsedPoseSearchTime = INFINITY;
	WantedPlayRate = 1.f;
	bJumpedToPose = false;
	ComponentDeltaYaw = 0.f;
	ComponentWorldYaw = FRotator(ComponentTransform.GetRotation()).Yaw;
	AnimationDeltaYaw = 0.f;

	PoseIndicesHistory.Reset();

#if UE_POSE_SEARCH_TRACE_ENABLED
	RootMotionTransformDelta = FTransform::Identity;
#endif // UE_POSE_SEARCH_TRACE_ENABLED
}

void FMotionMatchingState::AdjustAssetTime(float AssetTime)
{
	CurrentSearchResult.Update(AssetTime);
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
			FInertializationRequest Request;
			Request.Duration = BlendTime;
#if ANIM_TRACE_ENABLED
			Request.Description = LOCTEXT("InertializationRequestDescription", "Motion Matching");
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

void FMotionMatchingState::JumpToPose(const FAnimationUpdateContext& Context, const UE::PoseSearch::FSearchResult& Result, int32 MaxActiveBlends, float BlendTime)
{
	// requesting inertial blending only if blendstack is disabled
	if (MaxActiveBlends <= 0)
	{
		RequestInertialBlend(Context, BlendTime);
	}

	// Remember which pose and sequence we're playing from the database
	CurrentSearchResult = Result;

	bJumpedToPose = true;
}

void FMotionMatchingState::UpdateWantedPlayRate(const UE::PoseSearch::FSearchContext& SearchContext, const FFloatInterval& PlayRate, float TrajectorySpeedMultiplier)
{
	if (CurrentSearchResult.IsValid())
	{
		if (!ensure(PlayRate.Min <= PlayRate.Max && PlayRate.Min > UE_KINDA_SMALL_NUMBER))
		{
			UE_LOG(LogPoseSearch, Error, TEXT("Couldn't update the WantedPlayRate in FMotionMatchingState::UpdateWantedPlayRate, because of invalid PlayRate interval (%f, %f)"), PlayRate.Min, PlayRate.Max);
			WantedPlayRate = 1.f;
		}
		else if (!FMath::IsNearlyEqual(PlayRate.Min, PlayRate.Max, UE_KINDA_SMALL_NUMBER))
		{
			if (const UE::PoseSearch::FFeatureVectorBuilder* PoseSearchFeatureVectorBuilder = SearchContext.GetCachedQuery(CurrentSearchResult.Database->Schema))
			{
				if (const UPoseSearchFeatureChannel_Trajectory* TrajectoryChannel = CurrentSearchResult.Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Trajectory>())
				{
					TConstArrayView<float> QueryData = PoseSearchFeatureVectorBuilder->GetValues();
					TConstArrayView<float> ResultData = CurrentSearchResult.Database->GetSearchIndex().GetPoseValues(CurrentSearchResult.PoseIdx);
					const float EstimatedSpeedRatio = TrajectoryChannel->GetEstimatedSpeedRatio(QueryData, ResultData);

					WantedPlayRate = FMath::Clamp(EstimatedSpeedRatio, PlayRate.Min, PlayRate.Max);
				}
				else
				{
					UE_LOG(LogPoseSearch, Warning,
						TEXT("Couldn't update the WantedPlayRate in FMotionMatchingState::UpdateWantedPlayRate, because Schema '%s' couldn't find a UPoseSearchFeatureChannel_Trajectory channel"),
						*GetNameSafe(CurrentSearchResult.Database->Schema));
				}
			}
		}
		else if (!FMath::IsNearlyZero(TrajectorySpeedMultiplier))
		{
			WantedPlayRate = PlayRate.Min / TrajectorySpeedMultiplier;
		}
		else
		{
			WantedPlayRate = PlayRate.Min;
		}
	}
}

void FMotionMatchingState::UpdateRootBoneControl(const FAnimationUpdateContext& Context, float YawFromAnimationBlendRate)
{
	const FAnimInstanceProxy* AnimInstanceProxy = Context.AnimInstanceProxy;

	const float CurrentComponentWorldYaw = FRotator(AnimInstanceProxy->GetComponentTransform().GetRotation()).Yaw;
	if (YawFromAnimationBlendRate < 0.f)
	{
		ComponentWorldYaw = CurrentComponentWorldYaw;
		ComponentDeltaYaw = 0.f;
	}
	else
	{
		// integrating MotionMatchingState.ComponentWorldYaw with a lerped value between 
		// the previous frame AnimationDeltaYaw and CurrentComponentDeltaYaw (component delta yaw happened during the delta time)
		const float CurrentComponentDeltaYaw = FRotator::NormalizeAxis(CurrentComponentWorldYaw - ComponentWorldYaw);

		// lerping the animation delta with the capsule delta
		const float LerpValue = FMath::Min(YawFromAnimationBlendRate * Context.GetDeltaTime(), 1.f);
		const float LerpedDeltaYaw = FMath::Lerp(AnimationDeltaYaw, CurrentComponentDeltaYaw, LerpValue);

		ComponentWorldYaw = FRotator::NormalizeAxis(ComponentWorldYaw + LerpedDeltaYaw);

		// @todo: handle the case when the character is on top of a rotating platform
		ComponentDeltaYaw = FRotator::NormalizeAxis(ComponentWorldYaw - CurrentComponentWorldYaw);
	}
}

#if UE_POSE_SEARCH_TRACE_ENABLED
void UPoseSearchLibrary::TraceMotionMatchingState(
	const FPoseSearchQueryTrajectory& Trajectory,
	UE::PoseSearch::FSearchContext& SearchContext,
	const UE::PoseSearch::FSearchResult& CurrentResult,
	float ElapsedPoseSearchTime,
	const FTransform& RootMotionTransformDelta,
	const UObject* AnimInstance,
	int32 NodeId,
	float DeltaTime,
	bool bSearch,
	float RecordingTime,
	float SearchBestCost,
	float SearchBruteForceCost,
	int32 SearchBestPosePos)
{
	using namespace UE::PoseSearch;
	
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
			DatabaseEntries[DbEntryIdx].QueryVector = SearchContext.GetOrBuildQuery(Database->Schema).GetValues();
		}

		return DbEntryIdx;
	};

	const int32 CurrentPoseIdx = bSearch && CurrentResult.PoseCost.IsValid() ? CurrentResult.PoseIdx : -1;
	FTraceMotionMatchingState TraceState;

	for (const UPoseSearchDatabase* SearchedDatabase : SearchContext.BestCandidates.GetSearchedDatabases())
	{
		AddUniqueDatabase(TraceState.DatabaseEntries, SearchedDatabase, SearchContext);
	}

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
		if (CurrentPoseIdx == PoseCandidate.PoseIdx && CurrentResult.Database.Get() == PoseCandidate.Database)
		{
			EnumAddFlags(PoseEntry.PoseCandidateFlags, EPoseCandidateFlags::Valid_CurrentPose);
			
			TraceState.CurrentDbEntryIdx = DbEntryIdx;
			TraceState.CurrentPoseEntryIdx = DbEntry.PoseEntries.Add(PoseEntry);
		}
		else
		{
			DbEntry.PoseEntries.Add(PoseEntry);
		}
	}

	if (DeltaTime > SMALL_NUMBER && SearchContext.IsTrajectoryValid())
	{
		// simulation
		const FTransform PrevRoot = SearchContext.GetRootAtTime(-DeltaTime);
		const FTransform CurrRoot = SearchContext.GetRootAtTime(0.f);
		const FTransform SimDelta = CurrRoot.GetRelativeTransform(PrevRoot);

		TraceState.SimLinearVelocity = SimDelta.GetTranslation().Size() / DeltaTime;
		TraceState.SimAngularVelocity = FMath::RadiansToDegrees(SimDelta.GetRotation().GetAngle()) / DeltaTime;

		// animation
		TraceState.AnimLinearVelocity = RootMotionTransformDelta.GetTranslation().Size() / DeltaTime;
		TraceState.AnimAngularVelocity = FMath::RadiansToDegrees(RootMotionTransformDelta.GetRotation().GetAngle()) / DeltaTime;
	}

	TraceState.ElapsedPoseSearchTime = ElapsedPoseSearchTime;
	TraceState.AssetPlayerTime = CurrentResult.AssetTime;
	TraceState.DeltaTime = DeltaTime;

	TraceState.RecordingTime = RecordingTime;
	TraceState.SearchBestCost = SearchBestCost;
	TraceState.SearchBruteForceCost = SearchBruteForceCost;
	TraceState.SearchBestPosePos = SearchBestPosePos;

	TraceState.Trajectory = Trajectory;

	TraceState.Output(AnimInstance, NodeId);
}
#endif // UE_POSE_SEARCH_TRACE_ENABLED

void UPoseSearchLibrary::UpdateMotionMatchingState(
	const FAnimationUpdateContext& Context,
	const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases,
	const FPoseSearchQueryTrajectory& Trajectory,
	float TrajectorySpeedMultiplier,
	float BlendTime,
	int32 MaxActiveBlends,
	float PoseJumpThresholdTime,
	float PoseReselectHistory,
	float SearchThrottleTime,
	const FFloatInterval& PlayRate,
	FMotionMatchingState& InOutMotionMatchingState,
	float YawFromAnimationBlendRate,
	float YawFromAnimationTrajectoryBlendTime,
	bool bForceInterrupt,
	bool bShouldSearch,
	bool bDebugDrawQuery,
	bool bDebugDrawCurResult)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_Update);

	using namespace UE::PoseSearch;

	if (Databases.IsEmpty())
	{
		Context.LogMessage(
			EMessageSeverity::Error,
			LOCTEXT("NoDatabases", "No database assets provided for motion matching."));
		return;
	}

	InOutMotionMatchingState.UpdateRootBoneControl(Context, YawFromAnimationBlendRate);

	const float DeltaTime = Context.GetDeltaTime();

	InOutMotionMatchingState.bJumpedToPose = false;

	const IPoseHistory* History = nullptr;
	if (IPoseHistoryProvider* PoseHistoryProvider = Context.GetMessage<IPoseHistoryProvider>())
	{
		History = &PoseHistoryProvider->GetPoseHistory();
	}
	
	const FPoseSearchQueryTrajectory TrajectoryRootSpace = ProcessTrajectory(Trajectory, Context.AnimInstanceProxy->GetComponentTransform(), InOutMotionMatchingState.ComponentDeltaYaw, YawFromAnimationTrajectoryBlendTime, TrajectorySpeedMultiplier);
	
	FMemMark Mark(FMemStack::Get());
	FSearchContext SearchContext(&TrajectoryRootSpace, History, 0.f, &InOutMotionMatchingState.PoseIndicesHistory, InOutMotionMatchingState.CurrentSearchResult, PoseJumpThresholdTime, bForceInterrupt);

	const bool bCanAdvance = InOutMotionMatchingState.CurrentSearchResult.CanAdvance(DeltaTime);

	// If we can't advance or enough time has elapsed since the last pose jump then search
	const bool bSearch = !bCanAdvance || (bShouldSearch && (InOutMotionMatchingState.ElapsedPoseSearchTime >= SearchThrottleTime));
	if (bSearch)
	{
		InOutMotionMatchingState.ElapsedPoseSearchTime = 0.f;

		// Evaluate continuing pose
		FSearchResult SearchResult;
		if (!SearchContext.IsForceInterrupt() && bCanAdvance)
		{
			SearchResult.PoseCost = SearchContext.GetCurrentResult().Database->SearchContinuingPose(SearchContext);
			SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
		}

		bool bJumpToPose = false;
		for (TObjectPtr<const UPoseSearchDatabase> Database : Databases)
		{
			if (ensure(Database))
			{
				FSearchResult NewSearchResult = Database->Search(SearchContext);
				if (NewSearchResult.PoseCost.GetTotalCost() < SearchResult.PoseCost.GetTotalCost())
				{
					bJumpToPose = true;
					SearchResult = NewSearchResult;
					SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
				}
			}
		}

#if UE_POSE_SEARCH_TRACE_ENABLED
		if (!SearchResult.BruteForcePoseCost.IsValid())
		{
			SearchResult.BruteForcePoseCost = SearchResult.PoseCost;
		}
#endif // UE_POSE_SEARCH_TRACE_ENABLED

		if (bJumpToPose)
		{
			InOutMotionMatchingState.JumpToPose(Context, SearchResult, MaxActiveBlends, BlendTime);
		}
		else
		{
			// copying few properties of SearchResult into CurrentSearchResult to facilitate debug drawing
#if UE_POSE_SEARCH_TRACE_ENABLED
			InOutMotionMatchingState.CurrentSearchResult.BruteForcePoseCost = SearchResult.BruteForcePoseCost;
#endif // UE_POSE_SEARCH_TRACE_ENABLED
			InOutMotionMatchingState.CurrentSearchResult.PoseCost = SearchResult.PoseCost;
		}
	}
	else
	{
		InOutMotionMatchingState.ElapsedPoseSearchTime += DeltaTime;
	}

	InOutMotionMatchingState.UpdateWantedPlayRate(SearchContext, PlayRate, TrajectorySpeedMultiplier);

	InOutMotionMatchingState.PoseIndicesHistory.Update(InOutMotionMatchingState.CurrentSearchResult, DeltaTime, PoseReselectHistory);

#if UE_POSE_SEARCH_TRACE_ENABLED
	// Record debugger details
	if (IsTracing(Context))
	{
		const UAnimInstance* AnimInstance = Cast<const UAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());

		const float SearchBestCost = InOutMotionMatchingState.CurrentSearchResult.PoseCost.GetTotalCost();
		const float SearchBruteForceCost = InOutMotionMatchingState.CurrentSearchResult.BruteForcePoseCost.GetTotalCost();
		TraceMotionMatchingState(Trajectory, SearchContext, InOutMotionMatchingState.CurrentSearchResult, InOutMotionMatchingState.ElapsedPoseSearchTime,
			InOutMotionMatchingState.RootMotionTransformDelta, Context.AnimInstanceProxy->GetAnimInstanceObject(), Context.GetCurrentNodeId(), DeltaTime, bSearch,
			AnimInstance ? FObjectTrace::GetWorldElapsedTime(AnimInstance->GetWorld()) : 0.f, SearchBestCost, SearchBruteForceCost, InOutMotionMatchingState.CurrentSearchResult.BestPosePos);
	}
#endif

#if WITH_EDITORONLY_DATA && ENABLE_ANIM_DEBUG
	const FSearchResult& CurResult = InOutMotionMatchingState.CurrentSearchResult;
	if ((bDebugDrawQuery || bDebugDrawCurResult) && CurResult.Database != nullptr)
	{
		const UPoseSearchDatabase* CurResultDatabase = CurResult.Database.Get();

#if WITH_EDITOR
		// in case we're still indexing MotionMatchingState.CurrentSearchResult.Database we Reset the MotionMatchingState
		if (!FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(CurResultDatabase, ERequestAsyncBuildFlag::ContinueRequest))
		{
		}
		else
#endif // WITH_EDITOR
		{
			const FRotator DeltaRotation(0.f, InOutMotionMatchingState.ComponentDeltaYaw, 0.f);
			const FTransform DeltaTransform(DeltaRotation);
			const FTransform RootBoneTransform = DeltaTransform * Context.AnimInstanceProxy->GetComponentTransform();

			if (bDebugDrawCurResult)
			{
				UE::PoseSearch::FDebugDrawParams DrawParams(Context.AnimInstanceProxy, RootBoneTransform, CurResultDatabase);
				DrawParams.DrawFeatureVector(CurResult.PoseIdx);
			}

			if (bDebugDrawQuery)
			{
				UE::PoseSearch::FDebugDrawParams DrawParams(Context.AnimInstanceProxy, RootBoneTransform, CurResultDatabase, EDebugDrawFlags::DrawQuery);
				DrawParams.DrawFeatureVector(SearchContext.GetOrBuildQuery(CurResultDatabase->Schema).GetValues());
			}
		}
	}
#endif
}

// transforms Trajectory from world space to root bone space, and scale it by TrajectorySpeedMultiplier
FPoseSearchQueryTrajectory UPoseSearchLibrary::ProcessTrajectory(const FPoseSearchQueryTrajectory& Trajectory, const FTransform& ComponentWorldTransform, float RootBoneDeltaYaw, float YawFromAnimationTrajectoryBlendTime, float TrajectorySpeedMultiplier)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_ProcessTrajectory);

	const float TrajectorySpeedMultiplierInv = FMath::IsNearlyZero(TrajectorySpeedMultiplier) ? 1.f : 1.f / TrajectorySpeedMultiplier;

	FPoseSearchQueryTrajectory TrajectoryRootSpace = Trajectory;
	const FTransform ToRootSpace = ComponentWorldTransform.Inverse();
	for (FPoseSearchQueryTrajectorySample& Sample : TrajectoryRootSpace.Samples)
	{
		Sample.AccumulatedSeconds *= TrajectorySpeedMultiplierInv;

		Sample.Position = ToRootSpace.TransformPosition(Sample.Position);

		const float BlendParam = YawFromAnimationTrajectoryBlendTime < UE_KINDA_SMALL_NUMBER ? 1 : FMath::Clamp(1.f - (Sample.AccumulatedSeconds - YawFromAnimationTrajectoryBlendTime) / YawFromAnimationTrajectoryBlendTime, 0.f, 1.f);
		const FQuat RootBoneDelta(FRotator(0.f, RootBoneDeltaYaw * BlendParam, 0.f));
		const FQuat ToRootSpaceRotation = ToRootSpace.GetRotation() * RootBoneDelta;
		Sample.Facing = ToRootSpaceRotation * Sample.Facing;
	}

	return TrajectoryRootSpace;
}

void UPoseSearchLibrary::MotionMatch(
	UAnimInstance* AnimInstance,
	const UPoseSearchDatabase* Database,
	const FPoseSearchQueryTrajectory Trajectory,
	float TrajectorySpeedMultiplier,
	const FName PoseHistoryName,
	UAnimationAsset*& SelectedAnimation,
	float& SelectedTime,
	bool& bLoop,
	bool& bIsMirrored,
	FVector& BlendParameters,
	float& SearchCost,
	const UAnimationAsset* FutureAnimation,
	float FutureAnimationStartTime,
	float TimeToFutureAnimationStart,
	const int DebugSessionUniqueIdentifier)
{
#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	class UAnimInstanceProxyProvider : public UAnimInstance
	{
	public:
		static FAnimInstanceProxy* GetAnimInstanceProxy(UAnimInstance* AnimInstance)
		{
			if (AnimInstance)
			{
				return &static_cast<UAnimInstanceProxyProvider*>(AnimInstance)->GetProxyOnAnyThread<FAnimInstanceProxy>();
			}
			return nullptr;
		}
	};
#endif //ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

	using namespace UE::Anim;
	using namespace UE::PoseSearch;

	static constexpr float FiniteDelta = 1 / 60.0f;

	SelectedAnimation = nullptr;
	SelectedTime = 0.f;
	bLoop = false;
	bIsMirrored = false;
	BlendParameters = FVector::ZeroVector;
	SearchCost = MAX_flt;

	if (Database && AnimInstance)
	{
		const FPoseSearchQueryTrajectory TrajectoryRootSpace = ProcessTrajectory(Trajectory, AnimInstance->GetOwningComponent()->GetComponentTransform(), 0.f, 0.f, TrajectorySpeedMultiplier);

		// ExtendedPoseHistory will hold future poses to match AssetSamplerBase (at FutureAnimationStartTime) TimeToFutureAnimationStart seconds in the future
		FExtendedPoseHistory ExtendedPoseHistory;
		if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(AnimInstance->GetClass()))
		{
			if (const FAnimSubsystem_Tag* TagSubsystem = AnimBlueprintClass->FindSubsystem<FAnimSubsystem_Tag>())
			{
				if (const FAnimNode_PoseSearchHistoryCollector_Base* PoseHistoryNode = TagSubsystem->FindNodeByTag<FAnimNode_PoseSearchHistoryCollector_Base>(PoseHistoryName, AnimInstance))
				{
					ExtendedPoseHistory.Init(&PoseHistoryNode->GetPoseHistory());
				}
			}
		}

		if (!ExtendedPoseHistory.IsInitialized())
		{
			if (FutureAnimation)
			{
				UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - Couldn't find pose history with name '%s'. FutureAnimation search will not be performed"), *PoseHistoryName.ToString());
			}
			else
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - Couldn't find pose history with name '%s'"), *PoseHistoryName.ToString());
			}
		}
		else if (FutureAnimation)
		{
			const FBoneContainer& BoneContainer = AnimInstance->GetRequiredBonesOnAnyThread();
			// @todo... add input BlendParameters to support sampling FutureAnimation blendspaces
			const FAnimationAssetSampler Sampler(FutureAnimation, FVector::ZeroVector);

			if (FutureAnimationStartTime < FiniteDelta)
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - provided FutureAnimationStartTime (%f) is too small to be able to calculate velocities. Clamping it to minimum value of %f"), FutureAnimationStartTime, FiniteDelta);
				FutureAnimationStartTime = FiniteDelta;
			}

			const float MinTimeToFutureAnimationStart = FiniteDelta + UE_KINDA_SMALL_NUMBER;
			if (TimeToFutureAnimationStart < MinTimeToFutureAnimationStart)
			{
				UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - provided TimeToFutureAnimationStart (%f) is too small. Clamping it to minimum value of %f"), TimeToFutureAnimationStart, MinTimeToFutureAnimationStart);
				TimeToFutureAnimationStart = MinTimeToFutureAnimationStart;
			}

			// extracting 2 poses to be able to calculate velocities
			for (int i = 0; i < 2; ++i)
			{
				const float ExtractionTime = FutureAnimationStartTime + (i - 1) * FiniteDelta;
				const float FutureAnimationTime = TimeToFutureAnimationStart + (i - 1) * FiniteDelta;

				FCompactPose Pose;
				Pose.SetBoneContainer(&BoneContainer);
				Sampler.ExtractPose(ExtractionTime, Pose);

				FCSPose<FCompactPose> ComponentSpacePose;
				ComponentSpacePose.InitPose(Pose);

				const FPoseSearchQueryTrajectorySample TrajectorySample = TrajectoryRootSpace.GetSampleAtTime(ExtractionTime);
				const FTransform& ComponentTransform = AnimInstance->GetOwningComponent()->GetComponentTransform();
				const FTransform FutureComponentTransform = TrajectorySample.GetTransform() * ComponentTransform;

				ExtendedPoseHistory.AddFuturePose(FutureAnimationTime, ComponentSpacePose, FutureComponentTransform);
			}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
			if (CVarAnimMotionMatchDrawHistoryEnable.GetValueOnAnyThread())
			{
				if (FAnimInstanceProxy* AnimInstanceProxy = UAnimInstanceProxyProvider::GetAnimInstanceProxy(AnimInstance))
				{
					ExtendedPoseHistory.DebugDraw(*AnimInstanceProxy);
				}
			}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
		}

		FMemMark Mark(FMemStack::Get());
		FSearchContext SearchContext(&TrajectoryRootSpace, ExtendedPoseHistory.IsInitialized() ? &ExtendedPoseHistory : nullptr, TimeToFutureAnimationStart);

		FSearchResult SearchResult = Database->Search(SearchContext);
		if (SearchResult.IsValid())
		{
			const FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset();
			if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = SearchResult.Database->GetAnimationAssetBase(*SearchIndexAsset))
			{
				SelectedAnimation = DatabaseAsset->GetAnimationAsset();
				SelectedTime = SearchResult.AssetTime;
				bLoop = DatabaseAsset->IsLooping();
				bIsMirrored = SearchIndexAsset->bMirrored;
				BlendParameters = SearchIndexAsset->BlendParameters;
				SearchCost = SearchResult.PoseCost.GetTotalCost();
			}
		}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
		if (SearchResult.IsValid())
		{
			FAnimInstanceProxy* AnimInstanceProxy = UAnimInstanceProxyProvider::GetAnimInstanceProxy(AnimInstance);
			if (CVarAnimMotionMatchDrawMatchEnable.GetValueOnAnyThread())
			{
				UE::PoseSearch::FDebugDrawParams DrawParams(AnimInstanceProxy, AnimInstanceProxy->GetComponentTransform(), SearchResult.Database.Get());
				DrawParams.DrawFeatureVector(SearchResult.PoseIdx);
			}

			if (CVarAnimMotionMatchDrawQueryEnable.GetValueOnAnyThread())
			{
				UE::PoseSearch::FDebugDrawParams DrawParams(AnimInstanceProxy, AnimInstanceProxy->GetComponentTransform(), SearchResult.Database.Get(), EDebugDrawFlags::DrawQuery);
				DrawParams.DrawFeatureVector(SearchContext.GetOrBuildQuery(SearchResult.Database->Schema).GetValues());
			}
		}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

#if UE_POSE_SEARCH_TRACE_ENABLED
		const float SearchBestCost = SearchResult.PoseCost.GetTotalCost();
		const float SearchBruteForceCost = SearchResult.BruteForcePoseCost.GetTotalCost();
		TraceMotionMatchingState(Trajectory, SearchContext, SearchResult, 0.f, FTransform::Identity, AnimInstance, DebugSessionUniqueIdentifier,
			AnimInstance->GetDeltaSeconds(), true, FObjectTrace::GetWorldElapsedTime(AnimInstance->GetWorld()), SearchBestCost, SearchBruteForceCost, SearchResult.BestPosePos);
#endif // UE_POSE_SEARCH_TRACE_ENABLED
	}
}

#undef LOCTEXT_NAMESPACE
