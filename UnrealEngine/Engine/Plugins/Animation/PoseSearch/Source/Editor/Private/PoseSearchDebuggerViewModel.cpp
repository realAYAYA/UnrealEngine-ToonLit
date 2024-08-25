// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDebuggerViewModel.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/MirrorDataTable.h"
#include "Engine/SkeletalMesh.h"
#include "IAnimationProvider.h"
#include "IGameplayProvider.h"
#include "InstancedStruct.h"
#include "IRewindDebugger.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Trace/PoseSearchTraceProvider.h"

namespace UE::PoseSearch
{

FDebuggerViewModel::FDebuggerViewModel(uint64 InAnimInstanceId)
	: AnimInstanceId(InAnimInstanceId)
{
}

FDebuggerViewModel::~FDebuggerViewModel()
{
	for (TPair<uint64, TWeakObjectPtr<AActor>>& DebugDrawActorPair : DebugDrawActors)
	{
		if (DebugDrawActorPair.Value.IsValid())
		{
			DebugDrawActorPair.Value->Destroy();
		}
	}
}

const FTraceMotionMatchingStateMessage* FDebuggerViewModel::GetMotionMatchingState() const
{
	if (MotionMatchingStates.IsValidIndex(ActiveMotionMatchingStateIdx))
	{
		return &MotionMatchingStates[ActiveMotionMatchingStateIdx];
	}
	return nullptr;
}

const UPoseSearchDatabase* FDebuggerViewModel::GetCurrentDatabase() const
{
	if (const FTraceMotionMatchingStateMessage* State = GetMotionMatchingState())
	{
		return State->GetCurrentDatabase();
	}
	return nullptr;
}

int32 FDebuggerViewModel::GetNodesNum() const
{
	return MotionMatchingStates.Num();
}

void FDebuggerViewModel::OnUpdate()
{
	MotionMatchingStates.Empty();

	// Get provider and validate
	const TraceServices::IAnalysisSession* Session = RewindDebugger.Get()->GetAnalysisSession();
	TraceServices::FAnalysisSessionReadScope SessionReadScope(*Session);

	const FTraceProvider* PoseSearchProvider = Session->ReadProvider<FTraceProvider>(FTraceProvider::ProviderName);
	const IAnimationProvider* AnimationProvider = Session->ReadProvider<IAnimationProvider>("AnimationProvider");
	const IGameplayProvider* GameplayProvider = Session->ReadProvider<IGameplayProvider>("GameplayProvider");
	if (!(PoseSearchProvider && AnimationProvider && GameplayProvider))
	{
		return;
	}
	const double TraceTime = RewindDebugger.Get()->CurrentTraceTime();
	TraceServices::FFrame Frame;
	ReadFrameProvider(*Session).GetFrameFromTime(TraceFrameType_Game, TraceTime, Frame);
	PoseSearchProvider->EnumerateMotionMatchingStateTimelines(AnimInstanceId, [&Frame, this](const FTraceProvider::FMotionMatchingStateTimeline& InTimeline)
	{
		const FTraceMotionMatchingStateMessage* Message = nullptr;

		InTimeline.EnumerateEvents(Frame.StartTime, Frame.EndTime, [&Message](double InStartTime, double InEndTime, const FTraceMotionMatchingStateMessage& InMessage)
		{
			Message = &InMessage;
			return TraceServices::EEventEnumerate::Stop;
		});
		if (Message)
		{
			check(Message->Roles.Num() == Message->SkeletalMeshComponentIds.Num());
			MotionMatchingStates.Add(*Message);
		}
	});
	
	// adding all the missing DebugDrawActors, and tracking them via UsedDebugDrawActorIds
	TSet<uint64> UsedDebugDrawActorIds;
	for (const FTraceMotionMatchingStateMessage& MotionMatchingState : MotionMatchingStates)
	{
		for (uint64 SkeletalMeshComponentId : MotionMatchingState.SkeletalMeshComponentIds)
		{
			UsedDebugDrawActorIds.Add(SkeletalMeshComponentId);

			bool bNeedToAdd = true;
			if (TWeakObjectPtr<AActor>* DebugDrawActor = DebugDrawActors.Find(SkeletalMeshComponentId))
			{
				if (DebugDrawActor->IsValid())
				{
					bNeedToAdd = false;
				}
			}

			if (bNeedToAdd)
			{
				UWorld* World = RewindDebugger.Get()->GetWorldToVisualize();
				FActorSpawnParameters ActorSpawnParameters;
				ActorSpawnParameters.bHideFromSceneOutliner = false;
				ActorSpawnParameters.ObjectFlags |= RF_Transient;
				AActor* DebugDrawActor = World->SpawnActor<AActor>(ActorSpawnParameters);
				DebugDrawActor->SetActorLabel(TEXT("PoseSearch"));
				UPoseSearchMeshComponent* DebugDrawMeshComponent = NewObject<UPoseSearchMeshComponent>(DebugDrawActor);
				DebugDrawActor->AddInstanceComponent(DebugDrawMeshComponent);
				DebugDrawMeshComponent->RegisterComponentWithWorld(World);

				DebugDrawActors.Add(SkeletalMeshComponentId) = DebugDrawActor;
			}
		}
	}

	// cleaning up the no longer used actors (with a SkeletalMeshComponentId not in UsedDebugDrawActorIds)
	for (auto DebugDrawActorsIt = DebugDrawActors.CreateIterator(); DebugDrawActorsIt; ++DebugDrawActorsIt)
	{
		if (!UsedDebugDrawActorIds.Find(DebugDrawActorsIt.Key()))
		{
			if (DebugDrawActorsIt.Value().IsValid())
			{
				DebugDrawActorsIt.Value()->Destroy();
			}
			DebugDrawActorsIt.RemoveCurrent();
		}
	}

	/** No active motion matching state as no messages were read */
	if (MotionMatchingStates.IsEmpty())
	{
		return;
	}

	for (TPair<uint64, TWeakObjectPtr<AActor>>& DebugDrawActorPair : DebugDrawActors)
	{
		const uint64 SkeletalMeshComponentId = DebugDrawActorPair.Key;
		if (AActor* DebugDrawActor = DebugDrawActorPair.Value.Get())
		{
			for (UActorComponent* ActorComponent : DebugDrawActor->GetInstanceComponents())
			{
				if (UPoseSearchMeshComponent* PoseSearchMeshComponent = Cast<UPoseSearchMeshComponent>(ActorComponent))
				{
					AnimationProvider->ReadSkeletalMeshPoseTimeline(SkeletalMeshComponentId, [&Frame, AnimationProvider, GameplayProvider, PoseSearchMeshComponent](const IAnimationProvider::SkeletalMeshPoseTimeline& TimelineData, bool bHasCurves)
						{
							TimelineData.EnumerateEvents(Frame.StartTime, Frame.EndTime, [AnimationProvider, GameplayProvider, PoseSearchMeshComponent](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& PoseMessage) -> TraceServices::EEventEnumerate
								{
									const FSkeletalMeshInfo* SkeletalMeshInfo = AnimationProvider->FindSkeletalMeshInfo(PoseMessage.MeshId);
									const FObjectInfo* SkeletalMeshObjectInfo = GameplayProvider->FindObjectInfo(PoseMessage.MeshId);
									if (!SkeletalMeshInfo || !SkeletalMeshObjectInfo)
									{
										return TraceServices::EEventEnumerate::Stop;
									}

									USkeletalMesh* SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SkeletalMeshObjectInfo->PathName)).LoadSynchronous();
									if (SkeletalMesh)
									{
										PoseSearchMeshComponent->SetSkinnedAssetAndUpdate(SkeletalMesh, true);
									}
									FTransform ComponentWorldTransform;
									// Active skeleton is simply the traced bone transforms
									TArray<FTransform>& ComponentSpaceTransforms = PoseSearchMeshComponent->GetEditableComponentSpaceTransforms();
									AnimationProvider->GetSkeletalMeshComponentSpacePose(PoseMessage, *SkeletalMeshInfo, ComponentWorldTransform, ComponentSpaceTransforms);

									check(ComponentWorldTransform.Equals(PoseMessage.ComponentToWorld));

									PoseSearchMeshComponent->Initialize(ComponentWorldTransform);

									return TraceServices::EEventEnumerate::Stop;
								});
						});
					break;
				}
			}
		}
	}
}

void FDebuggerViewModel::OnUpdateNodeSelection(int32 InNodeId)
{
	if (InNodeId != INDEX_NONE)
	{
		// Find node in all motion matching states this frame
		ActiveMotionMatchingStateIdx = INDEX_NONE;
		const int32 NodesNum = MotionMatchingStates.Num();
		for (int32 i = 0; i < NodesNum; ++i)
		{
			if (MotionMatchingStates[i].NodeId == InNodeId)
			{
				ActiveMotionMatchingStateIdx = i;
				break;
			}
		}
	}
}

} // namespace UE::PoseSearch
