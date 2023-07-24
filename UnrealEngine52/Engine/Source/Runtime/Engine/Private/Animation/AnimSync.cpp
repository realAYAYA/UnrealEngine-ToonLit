// Copyright Epic Games, Inc. All Rights Reserved.

#include "Animation/AnimSync.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimStats.h"
#include "AnimationUtils.h"
#include "Animation/MirrorDataTable.h"
#include "AnimationRuntime.h"
#include "Misc/StringFormatArg.h"
#include "Stats/StatsHierarchical.h"

namespace UE { namespace Anim {

const FName FAnimSync::Attribute("Sync");

void FAnimSync::Reset()
{
	TArray<FAnimTickRecord>& UngroupedActivePlayers = UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()];
	UngroupedActivePlayers.Reset();

	FSyncGroupMap& SyncGroups = SyncGroupMaps[GetSyncGroupWriteIndex()];
	for (auto& SyncGroupPair : SyncGroups)
	{
		SyncGroupPair.Value.Reset();
	}
}

void FAnimSync::AddTickRecord(const FAnimTickRecord& InTickRecord, const FAnimSyncParams& InSyncParams)
{
	if (InSyncParams.GroupName != NAME_None)
	{
		FSyncGroupMap& SyncGroupMap = SyncGroupMaps[GetSyncGroupWriteIndex()];
		FAnimGroupInstance& SyncGroupInstance = SyncGroupMap.FindOrAdd(InSyncParams.GroupName);
		SyncGroupInstance.ActivePlayers.Add(InTickRecord);
		SyncGroupInstance.ActivePlayers.Top().MirrorDataTable = MirrorDataTable;
		SyncGroupInstance.TestTickRecordForLeadership(InSyncParams.Role);
	}
	else
	{
		UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()].Add(InTickRecord);
		UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()].Top().MirrorDataTable = MirrorDataTable;
	}
}

void FAnimSync::SetMirror(const UMirrorDataTable* MirrorTable)
{
	MirrorDataTable = MirrorTable;
}

void FAnimSync::TickAssetPlayerInstances(FAnimInstanceProxy& InProxy, float InDeltaSeconds)
{
	DECLARE_SCOPE_HIERARCHICAL_COUNTER_FUNC()

	SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstances);

	auto AccumulateRootMotion = [&InProxy](FAnimTickRecord& TickRecord, FAnimAssetTickContext& TickContext)
	{
		if (TickRecord.MirrorDataTable)
		{
			FTransform InTransform = TickContext.RootMotionMovementParams.GetRootMotionTransform();
			FVector T = InTransform.GetTranslation();
			T = FAnimationRuntime::MirrorVector(T, TickRecord.MirrorDataTable->MirrorAxis);

			FQuat Q = InTransform.GetRotation();
			Q = FAnimationRuntime::MirrorQuat(Q, TickRecord.MirrorDataTable->MirrorAxis);

			FVector S = InTransform.GetScale3D();
			FTransform MirroredTransform = FTransform(Q, T, S);
			InProxy.ExtractedRootMotion.AccumulateWithBlend(MirroredTransform, TickRecord.GetRootMotionWeight());
		}
		else
		{
			InProxy.ExtractedRootMotion.AccumulateWithBlend(TickContext.RootMotionMovementParams, TickRecord.GetRootMotionWeight());
		}
	};

	const ERootMotionMode::Type RootMotionMode = InProxy.GetRootMotionMode();

	// Handle all players inside sync groups
	FSyncGroupMap& SyncGroupMap = SyncGroupMaps[GetSyncGroupWriteIndex()];
	const FSyncGroupMap& PreviousSyncGroupMap = SyncGroupMaps[GetSyncGroupReadIndex()];
	TArray<FAnimTickRecord>& UngroupedActivePlayers = UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()];
	const TArray<FAnimTickRecord>& PreviousUngroupedActivePlayers = UngroupedActivePlayerArrays[GetSyncGroupReadIndex()];

	for (auto& SyncGroupPair : SyncGroupMap)
	{
		FAnimGroupInstance& SyncGroup = SyncGroupPair.Value;
	
		if (SyncGroup.ActivePlayers.Num() > 0)
		{
			const FAnimGroupInstance* PreviousGroup = PreviousSyncGroupMap.Find(SyncGroupPair.Key);
			SyncGroup.Prepare(PreviousGroup);

			UE_LOG(LogAnimMarkerSync, Log, TEXT("Ticking Group [%s] GroupLeader [%d]"), *SyncGroupPair.Key.ToString(), SyncGroup.GroupLeaderIndex);

			const bool bOnlyOneAnimationInGroup = SyncGroup.ActivePlayers.Num() == 1;

			// Tick the group leader
			FAnimAssetTickContext TickContext(InDeltaSeconds, RootMotionMode, bOnlyOneAnimationInGroup, SyncGroup.ValidMarkers);
			if (PreviousGroup)
			{
				// Initialize the anim position ratio from the previous frame's final anim position ratio in case we're not using marker based sync
				TickContext.SetPreviousAnimationPositionRatio(PreviousGroup->AnimLengthRatio);
				TickContext.SetAnimationPositionRatio(PreviousGroup->AnimLengthRatio);

				const FMarkerSyncAnimPosition& EndPosition = PreviousGroup->MarkerTickContext.GetMarkerSyncEndPosition();
				if ( EndPosition.IsValid() &&
						(EndPosition.PreviousMarkerName == NAME_None || SyncGroup.ValidMarkers.Contains(EndPosition.PreviousMarkerName)) &&
						(EndPosition.NextMarkerName == NAME_None || SyncGroup.ValidMarkers.Contains(EndPosition.NextMarkerName)))
				{
					TickContext.MarkerTickContext.SetMarkerSyncStartPosition(EndPosition);
				}
			}

#if DO_CHECK
			//For debugging UE-54705
			FName InitialMarkerPrevious = TickContext.MarkerTickContext.GetMarkerSyncStartPosition().PreviousMarkerName;
			FName InitialMarkerEnd = TickContext.MarkerTickContext.GetMarkerSyncStartPosition().NextMarkerName;
			const bool bIsLeaderRecordValidPre = SyncGroup.ActivePlayers[0].MarkerTickRecord->IsValid(SyncGroup.ActivePlayers[0].bLooping);
			FMarkerTickRecord LeaderPreMarkerTickRecord = *SyncGroup.ActivePlayers[0].MarkerTickRecord;
#endif

			// initialize to invalidate first
			ensureMsgf(SyncGroup.GroupLeaderIndex == INDEX_NONE, TEXT("SyncGroup %s had a non -1 group leader index of %d in asset %s"), *SyncGroupPair.Key.ToString(), SyncGroup.GroupLeaderIndex, *GetNameSafe(InProxy.GetAnimInstanceObject()));
			int32 GroupLeaderIndex = 0;
			for (; GroupLeaderIndex < SyncGroup.ActivePlayers.Num(); ++GroupLeaderIndex)
			{
				FAnimTickRecord& GroupLeader = SyncGroup.ActivePlayers[GroupLeaderIndex];
				// if it has leader score
				SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstance);
				FScopeCycleCounterUObject Scope(GroupLeader.SourceAsset);

				auto TickRecordMatchesGroupLeader = [&GroupLeader](const FAnimTickRecord& TickRecord) -> bool
				{
					return TickRecord.SourceAsset == GroupLeader.SourceAsset && TickRecord.TimeAccumulator == GroupLeader.TimeAccumulator;
				};

				// if the group leader was previously inactive then set it to resynchronize to the sync group's time
				// (maintains sync during inertialization or zero-length blends where the previous sync leader is no longer active)
				const bool bGroupLeaderWasActive = (PreviousGroup && PreviousGroup->ActivePlayers.ContainsByPredicate(TickRecordMatchesGroupLeader)) ||
					(PreviousUngroupedActivePlayers.ContainsByPredicate(TickRecordMatchesGroupLeader));
				const bool bGroupWasActive = PreviousGroup && !PreviousGroup->ActivePlayers.IsEmpty();
				TickContext.SetResyncToSyncGroup(!bGroupLeaderWasActive && bGroupWasActive);

				TickContext.MarkerTickContext.MarkersPassedThisTick.Reset();
				TickContext.RootMotionMovementParams.Clear();
				GroupLeader.SourceAsset->TickAssetPlayer(GroupLeader, InProxy.NotifyQueue, TickContext);

				if (RootMotionMode == ERootMotionMode::RootMotionFromEverything && TickContext.RootMotionMovementParams.bHasRootMotion)
				{
					AccumulateRootMotion(GroupLeader, TickContext);
				}

				// if we're not using marker based sync, we don't care, get out
				if (TickContext.CanUseMarkerPosition() == false)
				{
					SyncGroup.PreviousAnimLengthRatio = TickContext.GetPreviousAnimationPositionRatio();
					SyncGroup.AnimLengthRatio = TickContext.GetAnimationPositionRatio();
					SyncGroup.GroupLeaderIndex = GroupLeaderIndex;
					break;
				}
				// otherwise, the new position should contain the valid position for end, otherwise, we don't know where to sync to
				else if (TickContext.MarkerTickContext.IsMarkerSyncEndValid())
				{
					// if this leader contains correct position, break
					SyncGroup.PreviousAnimLengthRatio = TickContext.GetPreviousAnimationPositionRatio();
					SyncGroup.AnimLengthRatio = TickContext.GetAnimationPositionRatio();
					SyncGroup.MarkerTickContext = TickContext.MarkerTickContext;
					SyncGroup.GroupLeaderIndex = GroupLeaderIndex;
					UE_LOG(LogAnimMarkerSync, Log, TEXT("Previous Sync Group Marker Tick Context :\n%s"), *SyncGroup.MarkerTickContext.ToString());
					UE_LOG(LogAnimMarkerSync, Log, TEXT("New Sync Group Marker Tick Context :\n%s"), *TickContext.MarkerTickContext.ToString());
					break;
				}
				else
				{
					SyncGroup.PreviousAnimLengthRatio = TickContext.GetPreviousAnimationPositionRatio();
					SyncGroup.AnimLengthRatio = TickContext.GetAnimationPositionRatio();
					SyncGroup.GroupLeaderIndex = GroupLeaderIndex;
					UE_LOG(LogAnimMarkerSync, Log, TEXT("Invalid position from Leader %d. Trying next leader"), GroupLeaderIndex);
				}
			} 

			check(SyncGroup.GroupLeaderIndex != INDEX_NONE);
			// we found leader
			SyncGroup.Finalize(PreviousGroup);

			if (TickContext.CanUseMarkerPosition())
			{
				const FMarkerSyncAnimPosition& MarkerStart = TickContext.MarkerTickContext.GetMarkerSyncStartPosition();
				FName SyncGroupName = SyncGroupPair.Key;
				FAnimTickRecord& GroupLeader = SyncGroup.ActivePlayers[SyncGroup.GroupLeaderIndex];
				FString LeaderAnimName = GroupLeader.SourceAsset->GetName();

				//  Updated logic in search for cause of UE-54705
				const bool bStartMarkerValid = (MarkerStart.PreviousMarkerName == NAME_None) || SyncGroup.ValidMarkers.Contains(MarkerStart.PreviousMarkerName);
				const bool bEndMarkerValid = (MarkerStart.NextMarkerName == NAME_None) || SyncGroup.ValidMarkers.Contains(MarkerStart.NextMarkerName);

				if (!bStartMarkerValid)
				{
#if DO_CHECK
					FString ErrorMsg = FString(TEXT("Prev Marker name not valid for sync group.\n"));
					ErrorMsg += FString::Format(TEXT("\tMarker {0} : SyncGroupName {1} : Leader {2}\n"), { MarkerStart.PreviousMarkerName.ToString(), SyncGroupName.ToString(), LeaderAnimName });
					ErrorMsg += FString::Format(TEXT("\tInitalPrev {0} : InitialNext {1} : GroupLeaderIndex {2}\n"), { InitialMarkerPrevious.ToString(), InitialMarkerEnd.ToString(), GroupLeaderIndex });
					ErrorMsg += FString::Format(TEXT("\tLeader (0 index) was originally valid: {0} | Record: {1}\n"), { bIsLeaderRecordValidPre, LeaderPreMarkerTickRecord.ToString() });
					ErrorMsg += FString::Format(TEXT("\t Valid Markers : {0}\n"), { SyncGroup.ValidMarkers.Num() });
					for (int32 MarkerIndex = 0; MarkerIndex < SyncGroup.ValidMarkers.Num(); ++MarkerIndex)
					{
						ErrorMsg += FString::Format(TEXT("\t\t{0}) '{1}'\n"), {MarkerIndex, SyncGroup.ValidMarkers[MarkerIndex].ToString()});
					}
					ensureMsgf(false, TEXT("%s"), *ErrorMsg);
#endif
					TickContext.InvalidateMarkerSync();
				}
				else if (!bEndMarkerValid)
				{
#if DO_CHECK
					FString ErrorMsg = FString(TEXT("Next Marker name not valid for sync group.\n"));
					ErrorMsg += FString::Format(TEXT("\tMarker {0} : SyncGroupName {1} : Leader {2}\n"), { MarkerStart.NextMarkerName.ToString(), SyncGroupName.ToString(), LeaderAnimName });
					ErrorMsg += FString::Format(TEXT("\tInitalPrev {0} : InitialNext {1} : GroupLeaderIndex {2}\n"), { InitialMarkerPrevious.ToString(), InitialMarkerEnd.ToString(), GroupLeaderIndex });
					ErrorMsg += FString::Format(TEXT("\tLeader (0 index) was originally valid: {0} | Record: {1}\n"), { bIsLeaderRecordValidPre, LeaderPreMarkerTickRecord.ToString() });
					ErrorMsg += FString::Format(TEXT("\t Valid Markers : {0}\n"), { SyncGroup.ValidMarkers.Num() });
					for (int32 MarkerIndex = 0; MarkerIndex < SyncGroup.ValidMarkers.Num(); ++MarkerIndex)
					{
						ErrorMsg += FString::Format(TEXT("\t\t{0}) '{1}'\n"), { MarkerIndex, SyncGroup.ValidMarkers[MarkerIndex].ToString() });
					}
					ensureMsgf(false, TEXT("%s"), *ErrorMsg);
#endif
					TickContext.InvalidateMarkerSync();
				}
			}

			// Update everything else to follow the leader, if there is more followers
			if (SyncGroup.ActivePlayers.Num() > GroupLeaderIndex + 1)
			{
				// if we don't have a good leader, no reason to convert to follower
				// tick as leader
				TickContext.ConvertToFollower();
				TickContext.SetResyncToSyncGroup(false);

				for (int32 TickIndex = GroupLeaderIndex + 1; TickIndex < SyncGroup.ActivePlayers.Num(); ++TickIndex)
				{
					FAnimTickRecord& AssetPlayer = SyncGroup.ActivePlayers[TickIndex];
					{
						SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstance);
						FScopeCycleCounterUObject Scope(AssetPlayer.SourceAsset);
						TickContext.RootMotionMovementParams.Clear();
						AssetPlayer.SourceAsset->TickAssetPlayer(AssetPlayer, InProxy.NotifyQueue, TickContext);
					}
					if (RootMotionMode == ERootMotionMode::RootMotionFromEverything && TickContext.RootMotionMovementParams.bHasRootMotion)
					{
						AccumulateRootMotion(AssetPlayer, TickContext);
					}
				}
			}

#if ANIM_TRACE_ENABLED
			for(const FPassedMarker& PassedMarker : TickContext.MarkerTickContext.MarkersPassedThisTick)
			{
				TRACE_ANIM_SYNC_MARKER(CastChecked<UAnimInstance>(InProxy.GetAnimInstanceObject()), PassedMarker);
			}
#endif
		}
	}
	
	// Handle the remaining ungrouped animation players
	for (int32 TickIndex = 0; TickIndex < UngroupedActivePlayers.Num(); ++TickIndex)
	{
		FAnimTickRecord& AssetPlayerToTick = UngroupedActivePlayers[TickIndex];
		const TArray<FName>* UniqueNames = AssetPlayerToTick.SourceAsset->GetUniqueMarkerNames();
		const TArray<FName>& ValidMarkers = UniqueNames ? *UniqueNames : FMarkerTickContext::DefaultMarkerNames;

		const bool bOnlyOneAnimationInGroup = true;
		FAnimAssetTickContext TickContext(InDeltaSeconds, RootMotionMode, bOnlyOneAnimationInGroup, ValidMarkers);
		{
			SCOPE_CYCLE_COUNTER(STAT_TickAssetPlayerInstance);
			FScopeCycleCounterUObject Scope(AssetPlayerToTick.SourceAsset);
			AssetPlayerToTick.SourceAsset->TickAssetPlayer(AssetPlayerToTick, InProxy.NotifyQueue, TickContext);
		}
		if (RootMotionMode == ERootMotionMode::RootMotionFromEverything && TickContext.RootMotionMovementParams.bHasRootMotion)
		{
			AccumulateRootMotion(AssetPlayerToTick, TickContext);
		}

#if ANIM_TRACE_ENABLED
		for(const FPassedMarker& PassedMarker : TickContext.MarkerTickContext.MarkersPassedThisTick)
		{
			TRACE_ANIM_SYNC_MARKER(CastChecked<UAnimInstance>(InProxy.GetAnimInstanceObject()), PassedMarker);
		}
#endif
	}

	// Flip buffers now we have ticked
	TickSyncGroupWriteIndex();
}

bool FAnimSync::GetTimeToClosestMarker(FName SyncGroup, FName MarkerName, float& OutMarkerTime) const
{
	const FSyncGroupMap& SyncGroupMap = SyncGroupMaps[GetSyncGroupReadIndex()];

	if (const FAnimGroupInstance* SyncGroupInstancePtr = SyncGroupMap.Find(SyncGroup))
	{
		if (SyncGroupInstancePtr->bCanUseMarkerSync && SyncGroupInstancePtr->ActivePlayers.IsValidIndex(SyncGroupInstancePtr->GroupLeaderIndex))
		{
			const FMarkerSyncAnimPosition& EndPosition = SyncGroupInstancePtr->MarkerTickContext.GetMarkerSyncEndPosition();
			const FAnimTickRecord& Leader = SyncGroupInstancePtr->ActivePlayers[SyncGroupInstancePtr->GroupLeaderIndex];
			if (EndPosition.PreviousMarkerName == MarkerName)
			{
				OutMarkerTime = Leader.MarkerTickRecord->PreviousMarker.TimeToMarker;
				return true;
			}
			else if (EndPosition.NextMarkerName == MarkerName)
			{
				OutMarkerTime = Leader.MarkerTickRecord->NextMarker.TimeToMarker;
				return true;
			}
		}
	}
	return false;
}

bool FAnimSync::HasMarkerBeenHitThisFrame(FName SyncGroup, FName MarkerName) const
{
	const FSyncGroupMap& SyncGroupMap = SyncGroupMaps[GetSyncGroupReadIndex()];

	if (const FAnimGroupInstance* SyncGroupInstancePtr = SyncGroupMap.Find(SyncGroup))
	{
		if (SyncGroupInstancePtr->bCanUseMarkerSync)
		{
			return SyncGroupInstancePtr->MarkerTickContext.MarkersPassedThisTick.ContainsByPredicate([&MarkerName](const FPassedMarker& PassedMarker) -> bool
			{
				return PassedMarker.PassedMarkerName == MarkerName;
			});
		}
	}
	return false;
}

bool FAnimSync::IsSyncGroupBetweenMarkers(FName InSyncGroupName, FName PreviousMarker, FName NextMarker, bool bRespectMarkerOrder) const
{
	const FMarkerSyncAnimPosition& SyncGroupPosition = GetSyncGroupPosition(InSyncGroupName);
	if ((SyncGroupPosition.PreviousMarkerName == PreviousMarker) && (SyncGroupPosition.NextMarkerName == NextMarker))
	{
		return true;
	}

	if (!bRespectMarkerOrder)
	{
		return ((SyncGroupPosition.PreviousMarkerName == NextMarker) && (SyncGroupPosition.NextMarkerName == PreviousMarker));
	}

	return false;
}

FMarkerSyncAnimPosition FAnimSync::GetSyncGroupPosition(FName InSyncGroupName) const
{
	const FSyncGroupMap& SyncGroupMap = SyncGroupMaps[GetSyncGroupReadIndex()];

	if (const FAnimGroupInstance* SyncGroupInstancePtr = SyncGroupMap.Find(InSyncGroupName))
	{
		if (SyncGroupInstancePtr->bCanUseMarkerSync && SyncGroupInstancePtr->MarkerTickContext.IsMarkerSyncEndValid())
		{
			return SyncGroupInstancePtr->MarkerTickContext.GetMarkerSyncEndPosition();
		}
		else
		{
			return FMarkerSyncAnimPosition(NAME_None, NAME_None, SyncGroupInstancePtr->AnimLengthRatio);
		}
	}

	return FMarkerSyncAnimPosition();
}

bool FAnimSync::IsSyncGroupValid(FName InSyncGroupName) const
{
	const FSyncGroupMap& SyncGroupMap = SyncGroupMaps[GetSyncGroupReadIndex()];

	if (const FAnimGroupInstance* SyncGroupInstancePtr = SyncGroupMap.Find(InSyncGroupName))
	{
		// If we don't use Markers, we're always valid.
		return (!SyncGroupInstancePtr->bCanUseMarkerSync || SyncGroupInstancePtr->MarkerTickContext.IsMarkerSyncEndValid());
	}

	// If we're querying a sync group that doesn't exist, treat this as invalid
	return false;
}

FAnimTickRecord& FAnimSync::CreateUninitializedTickRecord(FAnimGroupInstance*& OutSyncGroupPtr, FName GroupName)
{
	// Find or create the sync group if there is one
	OutSyncGroupPtr = nullptr;
	if (GroupName != NAME_None)
	{
		FSyncGroupMap& SyncGroupMap = SyncGroupMaps[GetSyncGroupWriteIndex()];
		OutSyncGroupPtr = &SyncGroupMap.FindOrAdd(GroupName);
	}

	// Create the record
	FAnimTickRecord* TickRecord = new ((OutSyncGroupPtr != nullptr) ? OutSyncGroupPtr->ActivePlayers : UngroupedActivePlayerArrays[GetSyncGroupWriteIndex()]) FAnimTickRecord();
	return *TickRecord;
}

FAnimTickRecord& FAnimSync::CreateUninitializedTickRecordInScope(FAnimInstanceProxy& InProxy, FAnimGroupInstance*& OutSyncGroupPtr, FName GroupName, EAnimSyncGroupScope Scope)
{
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (GroupName != NAME_None)
	{
		// If we have no main proxy or it is "this", force us to local
		if(InProxy.GetMainInstanceProxy() == nullptr || InProxy.GetMainInstanceProxy() == &InProxy)
		{
			Scope = EAnimSyncGroupScope::Local;
		}

		switch(Scope)
		{
		default:
			ensureMsgf(false, TEXT("FAnimSync::CreateUninitializedTickRecordInScope: Scope has invalid value %d"), Scope);
			// Fall through

		case EAnimSyncGroupScope::Local:
			return CreateUninitializedTickRecord(OutSyncGroupPtr, GroupName);
		case EAnimSyncGroupScope::Component:
			// Forward to the main instance to sync with animations there in TickAssetPlayerInstances()
			return InProxy.GetMainInstanceProxy()->CreateUninitializedTickRecord(OutSyncGroupPtr, GroupName);
		}
	}
	
	return CreateUninitializedTickRecord(OutSyncGroupPtr, GroupName);
PRAGMA_ENABLE_DEPRECATION_WARNINGS
}

void FAnimSync::AddReferencedObjects(UAnimInstance* InAnimInstance, FReferenceCollector& Collector)
{
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(UngroupedActivePlayerArrays); ++Index)
	{
		TArray<FAnimTickRecord>& UngroupedPlayers = UngroupedActivePlayerArrays[Index];
		for (FAnimTickRecord& TickRecord : UngroupedPlayers)
		{
			Collector.AddReferencedObject(TickRecord.SourceAsset, InAnimInstance);
		}

		FSyncGroupMap& SyncGroupMap = SyncGroupMaps[Index];
		for(TPair<FName, FAnimGroupInstance>& GroupPair : SyncGroupMap)
		{
			for (FAnimTickRecord& TickRecord : GroupPair.Value.ActivePlayers)
			{
				Collector.AddReferencedObject(TickRecord.SourceAsset, InAnimInstance);
			}
		}
	}
}

}}	// namespace UE::Anim
