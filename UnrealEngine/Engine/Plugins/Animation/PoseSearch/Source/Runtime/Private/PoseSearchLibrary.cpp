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
#include "GameFramework/Character.h"
#include "InstancedStruct.h"
#include "PoseSearch/AnimNode_MotionMatching.h"
#include "PoseSearch/AnimNode_PoseSearchHistoryCollector.h"
#include "PoseSearch/PoseSearchAnimNotifies.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchMultiSequence.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchFeatureChannel_Trajectory.h"
#include "PoseSearch/Trace/PoseSearchTraceLogger.h"
#include "PoseSearchFeatureChannel_PermutationTime.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PoseSearchLibrary)

#define LOCTEXT_NAMESPACE "PoseSearchLibrary"

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
TAutoConsoleVariable<bool> CVarAnimMotionMatchDrawQueryEnable(TEXT("a.MotionMatch.DrawQuery.Enable"), false, TEXT("Enable / Disable MotionMatch Draw Query"));
TAutoConsoleVariable<bool> CVarAnimMotionMatchDrawMatchEnable(TEXT("a.MotionMatch.DrawMatch.Enable"), false, TEXT("Enable / Disable MotionMatch Draw Match"));
#endif

namespace UE::PoseSearch
{
	static bool IsForceInterrupt(EPoseSearchInterruptMode InterruptMode, const UPoseSearchDatabase* CurrentResultDatabase, const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases)
	{
		switch (InterruptMode)
		{
		case EPoseSearchInterruptMode::DoNotInterrupt:
			return false;

		case EPoseSearchInterruptMode::InterruptOnDatabaseChange:	// Fall through
		case EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose:
			return !Databases.Contains(CurrentResultDatabase);

		case EPoseSearchInterruptMode::ForceInterrupt:				// Fall through
		case EPoseSearchInterruptMode::ForceInterruptAndInvalidateContinuingPose:
			return true;

		default:
			checkNoEntry();
			return false;
		}
	}

	static bool IsInvalidatingContinuingPose(EPoseSearchInterruptMode InterruptMode, const UPoseSearchDatabase* CurrentResultDatabase, const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases)
	{
		switch (InterruptMode)
		{
		case EPoseSearchInterruptMode::DoNotInterrupt:				// Fall through
		case EPoseSearchInterruptMode::InterruptOnDatabaseChange:	// Fall through
		case EPoseSearchInterruptMode::ForceInterrupt:	
			return false;

		case EPoseSearchInterruptMode::InterruptOnDatabaseChangeAndInvalidateContinuingPose:
			return !Databases.Contains(CurrentResultDatabase);

		case EPoseSearchInterruptMode::ForceInterruptAndInvalidateContinuingPose:
			return true;

		default:
			checkNoEntry();
			return false;
		}
	}

	static bool ShouldUseCachedChannelData(const UPoseSearchDatabase* CurrentResultDatabase, const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases)
	{
		const UPoseSearchSchema* OneOfTheSchemas = nullptr;
		if (CurrentResultDatabase)
		{
			OneOfTheSchemas = CurrentResultDatabase->Schema;
		}

		for (const TObjectPtr<const UPoseSearchDatabase>& Database : Databases)
		{
			if (ensure(Database))
			{
				if (OneOfTheSchemas != Database->Schema)
				{
					if (OneOfTheSchemas == nullptr)
					{
						OneOfTheSchemas = Database->Schema;
					}
					else
					{
						// we found we need to search multiple schemas
						return true;
					}
				}
			}
		}

		return false;
	}

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
}

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

void FMotionMatchingState::JumpToPose(const FAnimationUpdateContext& Context, const UE::PoseSearch::FSearchResult& Result, int32 MaxActiveBlends, float BlendTime)
{
	// Remember which pose and sequence we're playing from the database
	CurrentSearchResult = Result;

	bJumpedToPose = true;
}

FVector FMotionMatchingState::GetEstimatedFutureRootMotionVelocity() const
{
	using namespace UE::PoseSearch;
	if (CurrentSearchResult.IsValid())
	{
		if (const UPoseSearchFeatureChannel_Trajectory* TrajectoryChannel = CurrentSearchResult.Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Trajectory>())
		{
			const FSearchIndex& SearchIndex = CurrentSearchResult.Database->GetSearchIndex();
			if (!SearchIndex.IsValuesEmpty())
			{
				TConstArrayView<float> ResultData = SearchIndex.GetPoseValues(CurrentSearchResult.PoseIdx);
				return TrajectoryChannel->GetEstimatedFutureRootMotionVelocity(ResultData);
			}
		}
	}

	return FVector::ZeroVector;
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
			TConstArrayView<float> QueryData = SearchContext.GetCachedQuery(CurrentSearchResult.Database->Schema);
			if (!QueryData.IsEmpty())
			{
				if (const UPoseSearchFeatureChannel_Trajectory* TrajectoryChannel = CurrentSearchResult.Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_Trajectory>())
				{
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

#if UE_POSE_SEARCH_TRACE_ENABLED
void UPoseSearchLibrary::TraceMotionMatchingState(
	UE::PoseSearch::FSearchContext& SearchContext,
	const UE::PoseSearch::FSearchResult& CurrentResult,
	float ElapsedPoseSearchTime,
	const FTransform& RootMotionTransformDelta,
	int32 NodeId,
	float DeltaTime,
	bool bSearch,
	float RecordingTime)
{
	using namespace UE::PoseSearch;
	
	FTraceMotionMatchingStateMessage TraceState;
	
	const int32 AnimInstancesNum = SearchContext.GetAnimInstances().Num();
	TraceState.SkeletalMeshComponentIds.SetNum(AnimInstancesNum);

	for (int32 AnimInstanceIndex = 0; AnimInstanceIndex < AnimInstancesNum; ++AnimInstanceIndex)
	{
		const UAnimInstance* AnimInstance = SearchContext.GetAnimInstances()[AnimInstanceIndex];
		const UObject* SkeletalMeshComponent = AnimInstance->GetOuter();

		TRACE_OBJECT(AnimInstance);

		TraceState.SkeletalMeshComponentIds[AnimInstanceIndex] = FObjectTrace::GetObjectId(SkeletalMeshComponent);
	}

	TraceState.Roles.SetNum(AnimInstancesNum);
	for (const FRoleToIndexPair& RoleToIndexPair : SearchContext.GetRoleToIndex())
	{
		TraceState.Roles[RoleToIndexPair.Value] = RoleToIndexPair.Key;
	}

	TraceState.PoseHistories.SetNum(AnimInstancesNum);
	for (int32 AnimInstanceIndex = 0; AnimInstanceIndex < AnimInstancesNum; ++AnimInstanceIndex)
	{
		TraceState.PoseHistories[AnimInstanceIndex].InitFrom(SearchContext.GetPoseHistories()[AnimInstanceIndex]);
	}

	int32 DbEntryIdx = 0;
	const int32 CurrentPoseIdx = bSearch && CurrentResult.PoseCost.IsValid() ? CurrentResult.PoseIdx : INDEX_NONE;
	TraceState.DatabaseEntries.SetNum(SearchContext.GetBestPoseCandidatesMap().Num());
	for (TPair<const UPoseSearchDatabase*, FSearchContext::FBestPoseCandidates> DatabaseBestPoseCandidates : SearchContext.GetBestPoseCandidatesMap())
	{
		const UPoseSearchDatabase* Database = DatabaseBestPoseCandidates.Key;
		check(Database);

		FTraceMotionMatchingStateDatabaseEntry& DbEntry = TraceState.DatabaseEntries[DbEntryIdx];

		// if throttling is on, the continuing pose can be valid, but no actual search occurred, so the query will not be cached, and we need to build it
		DbEntry.QueryVector = SearchContext.GetOrBuildQuery(Database->Schema);
		DbEntry.DatabaseId = FTraceMotionMatchingStateMessage::GetIdFromObject(Database);

		for (int32 CandidateIdx = 0; CandidateIdx < DatabaseBestPoseCandidates.Value.Num(); ++CandidateIdx)
		{
			const FSearchContext::FPoseCandidate PoseCandidate = DatabaseBestPoseCandidates.Value.GetUnsortedCandidate(CandidateIdx);

			FTraceMotionMatchingStatePoseEntry PoseEntry;
			PoseEntry.DbPoseIdx = PoseCandidate.PoseIdx;
			PoseEntry.Cost = PoseCandidate.Cost;
			PoseEntry.PoseCandidateFlags = PoseCandidate.PoseCandidateFlags;
			if (CurrentPoseIdx == PoseCandidate.PoseIdx && CurrentResult.Database.Get() == Database)
			{
				check(EnumHasAnyFlags(PoseEntry.PoseCandidateFlags, EPoseCandidateFlags::Valid_Pose | EPoseCandidateFlags::Valid_ContinuingPose));

				EnumAddFlags(PoseEntry.PoseCandidateFlags, EPoseCandidateFlags::Valid_CurrentPose);

				TraceState.CurrentDbEntryIdx = DbEntryIdx;
				TraceState.CurrentPoseEntryIdx = DbEntry.PoseEntries.Add(PoseEntry);
			}
			else
			{
				DbEntry.PoseEntries.Add(PoseEntry);
			}
		}

		++DbEntryIdx;
	}

	if (DeltaTime > SMALL_NUMBER)
	{
		// simulation
		if (SearchContext.AnyCachedQuery())
		{
			TraceState.SimLinearVelocity = 0.f;
			TraceState.SimAngularVelocity = 0.f;

			const int32 NumRoles = SearchContext.GetRoleToIndex().Num();
			for (const FRoleToIndexPair& RoleToIndexPair : SearchContext.GetRoleToIndex())
			{
				const FRole& Role = RoleToIndexPair.Key;

				const FTransform PrevRoot = SearchContext.GetWorldBoneTransformAtTime(-DeltaTime, Role, RootSchemaBoneIdx);
				const FTransform CurrRoot = SearchContext.GetWorldBoneTransformAtTime(0.f, Role, RootSchemaBoneIdx);
				
				const FTransform SimDelta = CurrRoot.GetRelativeTransform(PrevRoot);
				TraceState.SimLinearVelocity += SimDelta.GetTranslation().Size() / (DeltaTime * NumRoles);
				TraceState.SimAngularVelocity += FMath::RadiansToDegrees(SimDelta.GetRotation().GetAngle()) / (DeltaTime * NumRoles);
			}
		}

		// animation
		TraceState.AnimLinearVelocity = RootMotionTransformDelta.GetTranslation().Size() / DeltaTime;
		TraceState.AnimAngularVelocity = FMath::RadiansToDegrees(RootMotionTransformDelta.GetRotation().GetAngle()) / DeltaTime;
	}

	TraceState.ElapsedPoseSearchTime = ElapsedPoseSearchTime;
	TraceState.AssetPlayerTime = CurrentResult.AssetTime;
	TraceState.DeltaTime = DeltaTime;

	TraceState.RecordingTime = RecordingTime;
	TraceState.SearchBestCost = CurrentResult.PoseCost.GetTotalCost();
	TraceState.SearchBruteForceCost = CurrentResult.BruteForcePoseCost.GetTotalCost();
	TraceState.SearchBestPosePos = CurrentResult.BestPosePos;

	TraceState.Cycle = FPlatformTime::Cycles64();
	TraceState.AnimInstanceId = FObjectTrace::GetObjectId(SearchContext.GetAnimInstances()[0]);
	TraceState.NodeId = NodeId;

	TraceState.Output();
}
#endif // UE_POSE_SEARCH_TRACE_ENABLED

void UPoseSearchLibrary::UpdateMotionMatchingState(
	const FAnimationUpdateContext& Context,
	const TArray<TObjectPtr<const UPoseSearchDatabase>>& Databases,
	float BlendTime,
	int32 MaxActiveBlends,
	const FFloatInterval& PoseJumpThresholdTime,
	float PoseReselectHistory,
	float SearchThrottleTime,
	const FFloatInterval& PlayRate,
	FMotionMatchingState& InOutMotionMatchingState,
	EPoseSearchInterruptMode InterruptMode,
	bool bShouldSearch,
	bool bShouldUseCachedChannelData,
	bool bDebugDrawQuery,
	bool bDebugDrawCurResult)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_PoseSearch_Update);

	using namespace UE::PoseSearch;

	check(Context.AnimInstanceProxy);

	if (Databases.IsEmpty())
	{
		Context.LogMessage(
			EMessageSeverity::Error,
			LOCTEXT("NoDatabases", "No database assets provided for motion matching."));
		return;
	}

	const float DeltaTime = Context.GetDeltaTime();

	InOutMotionMatchingState.bJumpedToPose = false;

	// used when YawFromAnimationBlendRate is greater than zero, by setting a future (YawFromAnimationTrajectoryBlendTime seconds ahead) root bone to the skeleton default
	const IPoseHistory* PoseHistory = nullptr;
	if (IPoseHistoryProvider* PoseHistoryProvider = Context.GetMessage<IPoseHistoryProvider>())
	{
		PoseHistory = &PoseHistoryProvider->GetPoseHistory();
	}

	FMemMark Mark(FMemStack::Get());
	const UAnimInstance* AnimInstance = Cast<const UAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
	check(AnimInstance);

	const UPoseSearchDatabase* CurrentResultDatabase = InOutMotionMatchingState.CurrentSearchResult.Database.Get();
	if (IsInvalidatingContinuingPose(InterruptMode, CurrentResultDatabase, Databases))
	{
		InOutMotionMatchingState.CurrentSearchResult.Reset();
	}

	FSearchContext SearchContext(0.f, &InOutMotionMatchingState.PoseIndicesHistory, InOutMotionMatchingState.CurrentSearchResult, PoseJumpThresholdTime);
	SearchContext.AddRole(DefaultRole, AnimInstance, PoseHistory);

	const bool bCanAdvance = InOutMotionMatchingState.CurrentSearchResult.CanAdvance(DeltaTime);

	// If we can't advance or enough time has elapsed since the last pose jump then search
	const bool bSearch = !bCanAdvance || (bShouldSearch && (InOutMotionMatchingState.ElapsedPoseSearchTime >= SearchThrottleTime));
	if (bSearch)
	{
		InOutMotionMatchingState.ElapsedPoseSearchTime = 0.f;
		const bool bForceInterrupt = IsForceInterrupt(InterruptMode, CurrentResultDatabase, Databases);
		const bool bSearchContinuingPose = !bForceInterrupt && bCanAdvance;

		// calculating if it's worth bUseCachedChannelData (if we potentially have to build query with multiple schemas)
		SearchContext.SetUseCachedChannelData(bShouldUseCachedChannelData && ShouldUseCachedChannelData(bSearchContinuingPose ? CurrentResultDatabase : nullptr, Databases));

		FSearchResult SearchResult;
		// Evaluate continuing pose
		if (bSearchContinuingPose)
		{
			SearchResult = CurrentResultDatabase->SearchContinuingPose(SearchContext);
			SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
		}

		bool bJumpToPose = false;
		for (const TObjectPtr<const UPoseSearchDatabase>& Database : Databases)
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

		
#if WITH_EDITOR
		// resetting CurrentSearchResult if any DDC indexing on the requested databases is still in progress
		if (SearchContext.IsAsyncBuildIndexInProgress())
		{
			InOutMotionMatchingState.CurrentSearchResult.Reset();
		}
#endif // WITH_EDITOR

#if !NO_LOGGING
		if (!SearchResult.IsValid())
		{
			TStringBuilder<1024> StringBuilder;
			StringBuilder << "UPoseSearchLibrary::UpdateMotionMatchingState invalid search result : ForceInterrupt [";
			StringBuilder << bForceInterrupt;
			StringBuilder << "], CanAdvance [";
			StringBuilder << bCanAdvance;
			StringBuilder << "], Indexing [";

#if WITH_EDITOR
			StringBuilder << SearchContext.IsAsyncBuildIndexInProgress();
//#else // WITH_EDITOR
			StringBuilder << false;
#endif // WITH_EDITOR

			StringBuilder << "], Databases [";

			for (int32 DatabaseIndex = 0; DatabaseIndex < Databases.Num(); ++DatabaseIndex)
			{
				StringBuilder << GetNameSafe(Databases[DatabaseIndex]);
				if (DatabaseIndex != Databases.Num() - 1)
				{
					StringBuilder << ", ";
				}
			}
			
			StringBuilder << "] ";

			FString String = StringBuilder.ToString();
			UE_LOG(LogPoseSearch, Warning, TEXT("%s"), *String);
		}
#endif // !NO_LOGGING

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

	// @todo: consider moving this into if (bSearch) to avoid calling SearchContext.GetCachedQuery if no search is required
	InOutMotionMatchingState.UpdateWantedPlayRate(SearchContext, PlayRate, PoseHistory ? PoseHistory->GetTrajectorySpeedMultiplier() : 1.f);

	InOutMotionMatchingState.PoseIndicesHistory.Update(InOutMotionMatchingState.CurrentSearchResult, DeltaTime, PoseReselectHistory);

#if UE_POSE_SEARCH_TRACE_ENABLED
	// Record debugger details
	if (IsTracing(Context))
	{
		TraceMotionMatchingState(SearchContext, InOutMotionMatchingState.CurrentSearchResult, InOutMotionMatchingState.ElapsedPoseSearchTime,
			InOutMotionMatchingState.RootMotionTransformDelta, Context.GetCurrentNodeId(), DeltaTime, bSearch,
			AnimInstance ? FObjectTrace::GetWorldElapsedTime(AnimInstance->GetWorld()) : 0.f);
	}
#endif // UE_POSE_SEARCH_TRACE_ENABLED

#if WITH_EDITORONLY_DATA && ENABLE_ANIM_DEBUG
	const FSearchResult& CurResult = InOutMotionMatchingState.CurrentSearchResult;
	if (bDebugDrawQuery || bDebugDrawCurResult)
	{
		const UPoseSearchDatabase* CurResultDatabase = CurResult.Database.Get();

#if WITH_EDITOR
		if (EAsyncBuildIndexResult::Success == FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(CurResultDatabase, ERequestAsyncBuildFlag::ContinueRequest))
#endif // WITH_EDITOR
		{
			FAnimInstanceProxy* AnimInstanceProxy = Context.AnimInstanceProxy;
			const TArrayView<FAnimInstanceProxy*> AnimInstanceProxies = MakeArrayView(&AnimInstanceProxy, 1);
			
			if (bDebugDrawCurResult)
			{
				FDebugDrawParams DrawParams(AnimInstanceProxies, SearchContext.GetPoseHistories(), SearchContext.GetRoleToIndex(), CurResultDatabase);
				DrawParams.DrawFeatureVector(CurResult.PoseIdx);
			}

			if (bDebugDrawQuery)
			{
				FDebugDrawParams DrawParams(AnimInstanceProxies, SearchContext.GetPoseHistories(), SearchContext.GetRoleToIndex(), CurResultDatabase, EDebugDrawFlags::DrawQuery);
				DrawParams.DrawFeatureVector(SearchContext.GetOrBuildQuery(CurResultDatabase->Schema));
			}
		}
	}
#endif
}

void UPoseSearchLibrary::MotionMatch(
	UAnimInstance* AnimInstance,
	TArray<UObject*> AssetsToSearch,
	const FName PoseHistoryName,
	FPoseSearchFutureProperties Future,
	FPoseSearchBlueprintResult& Result,
	const int32 DebugSessionUniqueIdentifier)
{
	using namespace UE::PoseSearch;

	FMemMark Mark(FMemStack::Get());

	TArray<UAnimInstance*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> AnimInstances;
	AnimInstances.Add(AnimInstance);

	TArray<FName, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> Roles;
	Roles.Add(UE::PoseSearch::DefaultRole);

	TArray<const UObject*>& AssetsToSearchConst = reinterpret_cast<TArray<const UObject*>&>(AssetsToSearch);
	MotionMatch(AnimInstances, Roles, AssetsToSearchConst, PoseHistoryName, Future, Result, DebugSessionUniqueIdentifier);
}

void UPoseSearchLibrary::MotionMatchMulti(
	TArray<ACharacter*> Characters,
	TArray<FName> Roles,
	TArray<UObject*> AssetsToSearch,
	const FName PoseHistoryName,
	FPoseSearchBlueprintResult& Result,
	const int32 DebugSessionUniqueIdentifier)
{
	using namespace UE::PoseSearch;

	FMemMark Mark(FMemStack::Get());

	TArray<UAnimInstance*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> AnimInstances;
	for (ACharacter* Character : Characters)
	{
		UAnimInstance* AnimInstance = nullptr;
		if (Character && Character->GetMesh())
		{
			AnimInstance = Character->GetMesh()->GetAnimInstance();
		}
		AnimInstances.Add(AnimInstance);
	}

	TArray<const UObject*>& AssetsToSearchConst = reinterpret_cast<TArray<const UObject*>&>(AssetsToSearch);
	MotionMatch(AnimInstances, Roles, AssetsToSearchConst, PoseHistoryName, FPoseSearchFutureProperties(), Result, DebugSessionUniqueIdentifier);
}

void UPoseSearchLibrary::MotionMatch(
	TArrayView<UAnimInstance*> AnimInstances,
	TArrayView<const UE::PoseSearch::FRole> Roles,
	TArrayView<const UObject*> AssetsToSearch,
	const FName PoseHistoryName,
	const FPoseSearchFutureProperties& Future,
	FPoseSearchBlueprintResult& Result,
	const int32 DebugSessionUniqueIdentifier)
{
	using namespace UE::Anim;
	using namespace UE::PoseSearch;

	Result.SelectedAnimation = nullptr;
	Result.SelectedTime = 0.f;
	Result.bLoop = false;
	Result.bIsMirrored = false;
	Result.BlendParameters = FVector::ZeroVector;
	Result.SelectedDatabase = nullptr;
	Result.SearchCost = MAX_flt;

	if (AnimInstances.IsEmpty() || AnimInstances.Num() != Roles.Num())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - invalid input AnimInstances or Roles"));
		return;
	}
	
	for (UAnimInstance* AnimInstance : AnimInstances)
	{
		if (!AnimInstance)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - null AnimInstances"));
			return;
		}

		if (!AnimInstance->CurrentSkeleton)
		{
			UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - null AnimInstances->CurrentSkeleton"));
			return;
		}
	}

	FMemMark Mark(FMemStack::Get());

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	TArray<FColor, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> HistoryCollectorColors;
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

	// MemStackPoseHistories will hold future poses to match AssetSamplerBase (at FutureAnimationStartTime) TimeToFutureAnimationStart seconds in the future
	TArray<FMemStackPoseHistory, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> MemStackPoseHistories;
	for (UAnimInstance* AnimInstance : AnimInstances)
	{
		if (IAnimClassInterface* AnimBlueprintClass = IAnimClassInterface::GetFromClass(AnimInstance->GetClass()))
		{
			if (const FAnimSubsystem_Tag* TagSubsystem = AnimBlueprintClass->FindSubsystem<FAnimSubsystem_Tag>())
			{
				if (const FAnimNode_PoseSearchHistoryCollector_Base* PoseHistoryNode = TagSubsystem->FindNodeByTag<FAnimNode_PoseSearchHistoryCollector_Base>(PoseHistoryName, AnimInstance))
				{
					MemStackPoseHistories.AddDefaulted_GetRef().Init(&PoseHistoryNode->GetPoseHistory());

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	#if WITH_EDITORONLY_DATA
					HistoryCollectorColors.Emplace(PoseHistoryNode->DebugColor.ToFColor(true));
	#else // WITH_EDITORONLY_DATA
					HistoryCollectorColors.Emplace(FColor::Red);
	#endif // WITH_EDITORONLY_DATA
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG && WITH_EDITORONLY_DATA
				}
			}
		}
	}

	if (MemStackPoseHistories.Num() != AnimInstances.Num())
	{
		UE_LOG(LogPoseSearch, Error, TEXT("UPoseSearchLibrary::MotionMatch - Couldn't find pose history with name '%s'"), *PoseHistoryName.ToString());
		return;
	}

	float FutureIntervalTime = Future.IntervalTime;
	if (Future.Animation)
	{
		float FutureAnimationTime = Future.AnimationTime;
		if (FutureAnimationTime < FiniteDelta)
		{
			UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - provided Future.AnimationTime (%f) is too small to be able to calculate velocities. Clamping it to minimum value of %f"), FutureAnimationTime, FiniteDelta);
			FutureAnimationTime = FiniteDelta;
		}

		const float MinFutureIntervalTime = FiniteDelta + UE_KINDA_SMALL_NUMBER;
		if (FutureIntervalTime < MinFutureIntervalTime)
		{
			UE_LOG(LogPoseSearch, Warning, TEXT("UPoseSearchLibrary::MotionMatch - provided TimeToFutureAnimationStart (%f) is too small. Clamping it to minimum value of %f"), FutureIntervalTime, MinFutureIntervalTime);
			FutureIntervalTime = MinFutureIntervalTime;
		}

		for (int32 RoleIndex = 0; RoleIndex < Roles.Num(); ++RoleIndex)
		{
			// extracting 2 poses to be able to calculate velocities
			FCSPose<FCompactPose> ComponentSpacePose;
			FCompactPose Pose;
			Pose.SetBoneContainer(&AnimInstances[0]->GetRequiredBonesOnAnyThread());

			// @todo: add input BlendParameters to support sampling FutureAnimation blendspaces and support for multi character
			const UAnimationAsset* AnimationAsset = Cast<UAnimationAsset>(Future.Animation);
			if (!AnimationAsset)
			{
				if (const UPoseSearchMultiSequence* MultiSequence = Cast<UPoseSearchMultiSequence>(Future.Animation))
				{
					AnimationAsset = MultiSequence->GetSequence(Roles[RoleIndex]);
				}
				else
				{
					checkNoEntry();
				}
			}

			const FAnimationAssetSampler Sampler(AnimationAsset);
			for (int32 i = 0; i < 2; ++i)
			{
				const float FuturePoseExtractionTime = FutureAnimationTime + (i - 1) * FiniteDelta;
				const float FuturePoseAnimationTime = FutureIntervalTime + (i - 1) * FiniteDelta;

				Sampler.ExtractPose(FuturePoseExtractionTime, Pose);
				ComponentSpacePose.InitPose(Pose);
				MemStackPoseHistories[RoleIndex].AddFuturePose(FuturePoseAnimationTime, ComponentSpacePose);
			}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
			if (FAnimInstanceProxy* AnimInstanceProxy = UAnimInstanceProxyProvider::GetAnimInstanceProxy(AnimInstances[RoleIndex]))
			{
				MemStackPoseHistories[RoleIndex].DebugDraw(*AnimInstanceProxy, HistoryCollectorColors[RoleIndex]);
			}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
		}
	}

	TArray<const UE::PoseSearch::IPoseHistory*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> PoseHistories;
	for (const FMemStackPoseHistory& MemStackPoseHistory : MemStackPoseHistories)
	{
		PoseHistories.Add(MemStackPoseHistory.GetThisOrPoseHistory());
	}

	const FSearchResult SearchResult = MotionMatch(AnimInstances, Roles, PoseHistories, AssetsToSearch, nullptr, 0.f, DebugSessionUniqueIdentifier);
	if (SearchResult.IsValid())
	{
		const UPoseSearchDatabase* Database = SearchResult.Database.Get();
		const FSearchIndexAsset* SearchIndexAsset = SearchResult.GetSearchIndexAsset();
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = Database->GetAnimationAssetBase(*SearchIndexAsset))
		{
			Result.SelectedAnimation = DatabaseAsset->GetAnimationAsset();
			Result.SelectedTime = SearchResult.AssetTime;
			Result.bLoop = SearchIndexAsset->IsLooping();
			Result.bIsMirrored = SearchIndexAsset->IsMirrored();
			Result.BlendParameters = SearchIndexAsset->GetBlendParameters();
			Result.SelectedDatabase = Database;
			Result.SearchCost = SearchResult.PoseCost.GetTotalCost();
			
			// figuring out the WantedPlayRate
			Result.WantedPlayRate = 1.f;
			if (Future.Animation)
			{
				if (const UPoseSearchFeatureChannel_PermutationTime* PermutationTimeChannel = Database->Schema->FindFirstChannelOfType<UPoseSearchFeatureChannel_PermutationTime>())
				{
					const FSearchIndex& SearchIndex = Database->GetSearchIndex();
					if (!SearchIndex.IsValuesEmpty())
					{
						TConstArrayView<float> ResultData = Database->GetSearchIndex().GetPoseValues(SearchResult.PoseIdx);
						const float ActualIntervalTime = PermutationTimeChannel->GetPermutationTime(ResultData);
						Result.WantedPlayRate = ActualIntervalTime / FutureIntervalTime;
					}
				}
			}
		}
	}
}

UE::PoseSearch::FSearchResult UPoseSearchLibrary::MotionMatch(
	const FAnimationBaseContext& Context,
	TArrayView<const UObject*> AssetsToSearch,
	const UObject* PlayingAsset,
	float PlayingAssetAccumulatedTime)
{
	using namespace UE::PoseSearch;

	const IPoseHistory* PoseHistory = nullptr;
	if (IPoseHistoryProvider* PoseHistoryProvider = Context.GetMessage<IPoseHistoryProvider>())
	{
		PoseHistory = &PoseHistoryProvider->GetPoseHistory();
	}

	UAnimInstance* AnimInstance = Cast<UAnimInstance>(Context.AnimInstanceProxy->GetAnimInstanceObject());
	check(AnimInstance);

	return MotionMatch(MakeArrayView(&AnimInstance, 1), MakeArrayView(&DefaultRole, 1), MakeArrayView(&PoseHistory, 1), AssetsToSearch, PlayingAsset, PlayingAssetAccumulatedTime, Context.GetCurrentNodeId());
}
	
UE::PoseSearch::FSearchResult UPoseSearchLibrary::MotionMatch(
	TArrayView<UAnimInstance*> AnimInstances,
	TArrayView<const UE::PoseSearch::FRole> Roles,
	TArrayView<const UE::PoseSearch::IPoseHistory*> PoseHistories, 
	TArrayView<const UObject*> AssetsToSearch,
	const UObject* PlayingAsset,
	float PlayingAssetAccumulatedTime,
	const int32 DebugSessionUniqueIdentifier)
{
	check(!AnimInstances.IsEmpty() && AnimInstances.Num() == Roles.Num() && AnimInstances.Num() == PoseHistories.Num());

	using namespace UE::PoseSearch;

	FSearchResult SearchResult;

	FMemMark Mark(FMemStack::Get());
	FSearchResult ReconstructedPreviousSearchResult;
	FSearchContext SearchContext(0.f, nullptr, ReconstructedPreviousSearchResult);

	// @todo: all assets in AssetsToSearch should have a consistent Roles requirements, or else the search will throw an error!
	for (int32 RoleIndex = 0; RoleIndex < Roles.Num(); ++RoleIndex)
	{
		SearchContext.AddRole(Roles[RoleIndex], AnimInstances[RoleIndex], PoseHistories[RoleIndex]);
	}

	// budgeting some stack allocations for simple use cases. bigger requests of AnimationAssets contining 
	// UAnimNotifyState_PoseSearchBranchIn referencing multiple datbases will default to slower heap allocations
	enum { MAX_STACK_ALLOCATED_ANIMATIONS = 16 };
	enum { MAX_STACK_ALLOCATED_SETS = 2 };
	typedef	TArray<const UObject*, TInlineAllocator<MAX_STACK_ALLOCATED_ANIMATIONS, TMemStackAllocator<>>> TAssetsToSearch;
	// an empty TAssetsToSearch associated to Database means we need to search ALL the assets
	typedef TMap<const UPoseSearchDatabase*, TAssetsToSearch, TInlineSetAllocator<MAX_STACK_ALLOCATED_SETS, TMemStackSetAllocator<>>> TAssetsToSearchPerDatabaseMap;
	typedef TPair<const UPoseSearchDatabase*, TAssetsToSearch> TAssetsToSearchPerDatabasePair;
	TAssetsToSearchPerDatabaseMap AssetsToSearchPerDatabaseMap;
	
	auto AddToSearch = [](TAssetsToSearchPerDatabaseMap& AssetsToSearchPerDatabaseMap, const UObject* AssetToSearch)
	{
		if (const UAnimSequenceBase* SequenceBase = Cast<const UAnimSequenceBase>(AssetToSearch))
		{
			for (const FAnimNotifyEvent& NotifyEvent : SequenceBase->Notifies)
			{
				if (const UAnimNotifyState_PoseSearchBranchIn* PoseSearchBranchIn = Cast<UAnimNotifyState_PoseSearchBranchIn>(NotifyEvent.NotifyStateClass))
				{
					if (PoseSearchBranchIn->Database)
					{
#if WITH_EDITOR
						if (!PoseSearchBranchIn->Database->Contains(SequenceBase))
						{
							UE_LOG(LogPoseSearch, Error, TEXT("improperly setup UAnimSequenceBase. Database %s doesn't contain UAnimSequenceBase %s"), *PoseSearchBranchIn->Database->GetName(), *SequenceBase->GetName());
						}
						else 
#endif // WITH_EDITOR
						if (TAssetsToSearch* AssetsToSearch = AssetsToSearchPerDatabaseMap.Find(PoseSearchBranchIn->Database))
						{
							// an empty TAssetsToSearch associated to Database means we need to search ALL the assets, so we don't need to add this SequenceBase
							if (!AssetsToSearch->IsEmpty())
							{
								AssetsToSearch->AddUnique(SequenceBase);
							}
						}
						else
						{
							AssetsToSearchPerDatabaseMap.Add(PoseSearchBranchIn->Database).AddUnique(SequenceBase);
						}
					}
					else
					{
						UE_LOG(LogPoseSearch, Error, TEXT("improperly setup UAnimNotifyState_PoseSearchBranchIn with null Database in %s"), *SequenceBase->GetName());
					}
				}
			}
		}
		else if (const UPoseSearchDatabase* Database = Cast<UPoseSearchDatabase>(AssetToSearch))
		{
			// an empty TAssetsToSearch associated to Database means we need to search ALL the assets
			AssetsToSearchPerDatabaseMap.FindOrAdd(Database).Reset();
		}
	};

	// collecting all the possible continuing pose search (it could be multiple searches, but most likely only one)
	const float DeltaSeconds = AnimInstances[0]->GetDeltaSeconds();
	if (const UAnimationAsset* PlayingAnimationAsset = Cast<UAnimationAsset>(PlayingAsset))
	{
		AddToSearch(AssetsToSearchPerDatabaseMap, PlayingAnimationAsset);
		for (const TAssetsToSearchPerDatabasePair& AssetsToSearchPerDatabasePair : AssetsToSearchPerDatabaseMap)
		{
			const UPoseSearchDatabase* Database = AssetsToSearchPerDatabasePair.Key;

#if WITH_EDITOR
			if (EAsyncBuildIndexResult::Success != FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
			{
				SearchContext.SetAsyncBuildIndexInProgress();
			}
			else
#endif // WITH_EDITOR
			{
				check(Database);

				const FSearchIndex& SearchIndex = Database->GetSearchIndex();
				for (int32 AssetIndex = 0; AssetIndex < SearchIndex.Assets.Num(); ++AssetIndex)
				{
					const FSearchIndexAsset& SearchIndexAsset = SearchIndex.Assets[AssetIndex];
					if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = Database->GetAnimationAssetBase(SearchIndexAsset))
					{
						if (PlayingAnimationAsset == DatabaseAnimationAssetBase->GetAnimationAsset())
						{
							bool bCanAdvance = true;
							if (!SearchIndexAsset.IsLooping())
							{
								const float FirstSampleTime = SearchIndexAsset.GetFirstSampleTime(Database->Schema->SampleRate);
								const float LastSampleTime = SearchIndexAsset.GetLastSampleTime(Database->Schema->SampleRate) - DeltaSeconds;
								const float MaxTimeToBeAbleToContinuingPlayingAnimation = LastSampleTime - DeltaSeconds;

								bCanAdvance = PlayingAssetAccumulatedTime >= FirstSampleTime && PlayingAssetAccumulatedTime < MaxTimeToBeAbleToContinuingPlayingAnimation;
							}

							if (bCanAdvance)
							{
								ReconstructedPreviousSearchResult.Database = Database;
								ReconstructedPreviousSearchResult.AssetTime = PlayingAssetAccumulatedTime;
								ReconstructedPreviousSearchResult.PoseIdx = Database->GetPoseIndexFromTime(PlayingAssetAccumulatedTime, SearchIndexAsset);
								SearchContext.UpdateCurrentResultPoseVector();

								const FSearchResult NewSearchResult = Database->SearchContinuingPose(SearchContext);
								if (NewSearchResult.PoseCost.GetTotalCost() < SearchResult.PoseCost.GetTotalCost())
								{
									SearchResult = NewSearchResult;
									SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
								}
							}
						}
					}
				}
			}
		}

		AssetsToSearchPerDatabaseMap.Reset();
	}

	// collecting all the other databases searches
	if (!AssetsToSearch.IsEmpty())
	{
		for (const UObject* AssetToSearch : AssetsToSearch)
		{
			AddToSearch(AssetsToSearchPerDatabaseMap, AssetToSearch);
		}

		for (const TAssetsToSearchPerDatabasePair& AssetsToSearchPerDatabasePair : AssetsToSearchPerDatabaseMap)
		{
			const UPoseSearchDatabase* Database = AssetsToSearchPerDatabasePair.Key;
			check(Database);

			SearchContext.SetAssetsToConsider(AssetsToSearchPerDatabasePair.Value);

			const FSearchResult NewSearchResult = Database->Search(SearchContext);
			if (NewSearchResult.PoseCost.GetTotalCost() < SearchResult.PoseCost.GetTotalCost())
			{
				SearchResult = NewSearchResult;
				SearchContext.UpdateCurrentBestCost(SearchResult.PoseCost);
			}
		}
	}

#if ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG
	if (SearchResult.IsValid())
	{
		const bool bDrawMatch = CVarAnimMotionMatchDrawMatchEnable.GetValueOnAnyThread();
		const bool bDrawquery = CVarAnimMotionMatchDrawQueryEnable.GetValueOnAnyThread();

		if (bDrawMatch || bDrawquery)
		{
			TArray<FAnimInstanceProxy*, TInlineAllocator<PreallocatedRolesNum, TMemStackAllocator<>>> AnimInstanceProxies;
			AnimInstanceProxies.SetNum(Roles.Num());
			
			for (int32 RoleIndex = 0; RoleIndex < Roles.Num(); ++RoleIndex)
			{
				AnimInstanceProxies[RoleIndex] = UAnimInstanceProxyProvider::GetAnimInstanceProxy(AnimInstances[RoleIndex]);
			}

			if (bDrawMatch)
			{
				FDebugDrawParams DrawParams(AnimInstanceProxies, SearchContext.GetPoseHistories(), SearchContext.GetRoleToIndex(), SearchResult.Database.Get());
				DrawParams.DrawFeatureVector(SearchResult.PoseIdx);
			}

			if (bDrawquery)
			{
				FDebugDrawParams DrawParams(AnimInstanceProxies, SearchContext.GetPoseHistories(), SearchContext.GetRoleToIndex(), SearchResult.Database.Get(), EDebugDrawFlags::DrawQuery);
				DrawParams.DrawFeatureVector(SearchContext.GetOrBuildQuery(SearchResult.Database->Schema));
			}
		}
	}
#endif // ENABLE_DRAW_DEBUG && ENABLE_ANIM_DEBUG

#if UE_POSE_SEARCH_TRACE_ENABLED
	const float SearchBestCost = SearchResult.PoseCost.GetTotalCost();
	const float SearchBruteForceCost = SearchResult.BruteForcePoseCost.GetTotalCost();
	TraceMotionMatchingState(SearchContext, SearchResult, 0.f, FTransform::Identity, DebugSessionUniqueIdentifier,
		DeltaSeconds, true, FObjectTrace::GetWorldElapsedTime(AnimInstances[0]->GetWorld()));
#endif // UE_POSE_SEARCH_TRACE_ENABLED

	return SearchResult;
}

#undef LOCTEXT_NAMESPACE
