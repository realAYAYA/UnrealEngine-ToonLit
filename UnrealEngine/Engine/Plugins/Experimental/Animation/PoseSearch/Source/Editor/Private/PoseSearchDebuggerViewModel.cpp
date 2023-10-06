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
	Skeletons.AddDefaulted(ESkeletonIndex::Num);
}

FDebuggerViewModel::~FDebuggerViewModel()
{
	for (FSkeleton& Skeleton : Skeletons)
	{
		if (Skeleton.Actor != nullptr)
		{
			Skeleton.Actor->Destroy();
		}
	}

	Skeletons.Empty();
}

const FTraceMotionMatchingStateMessage* FDebuggerViewModel::GetMotionMatchingState() const
{
	return ActiveMotionMatchingState;
}

const UPoseSearchDatabase* FDebuggerViewModel::GetCurrentDatabase() const
{
	return ActiveMotionMatchingState ? ActiveMotionMatchingState->GetCurrentDatabase() : nullptr;
}

void FDebuggerViewModel::ShowSelectedSkeleton(const UPoseSearchDatabase* Database, int32 DbPoseIdx, float Time)
{
	UPoseSearchMeshComponent* Component = Skeletons[SelectedPose].Component.Get();
	if (!Component)
	{
		return;
	}

	if (!FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
	{
		return;
	}

	const FSearchIndex& SearchIndex = Database->GetSearchIndex();
	if (const FSearchIndexAsset* IndexAsset = SearchIndex.GetAssetForPoseSafe(DbPoseIdx))
	{
		Component->ResetToStart();
		bSelecting = true;

		Skeletons[SelectedPose].Time = Time;
		Skeletons[SelectedPose].bMirrored = IndexAsset->bMirrored;
		Skeletons[SelectedPose].SourceDatabase = Database;
		Skeletons[SelectedPose].AssetIdx = IndexAsset->SourceAssetIdx;
		Skeletons[SelectedPose].BlendParameters = IndexAsset->BlendParameters;
	}
}

void FDebuggerViewModel::ClearSelectedSkeleton()
{
	bSelecting = false;
}

const TArray<int32>* FDebuggerViewModel::GetNodeIds() const
{
	return &NodeIds;
}

int32 FDebuggerViewModel::GetNodesNum() const
{
	return MotionMatchingStates.Num();
}

const FTransform& FDebuggerViewModel::GetRootBoneTransform() const
{
	return RootBoneWorldTransform;
}

void FDebuggerViewModel::OnUpdate()
{
	if (!bSkeletonsInitialized)
	{
		UWorld* World = RewindDebugger.Get()->GetWorldToVisualize();
		for (FSkeleton& Skeleton : Skeletons)
		{
			FActorSpawnParameters ActorSpawnParameters;
			ActorSpawnParameters.bHideFromSceneOutliner = false;
			ActorSpawnParameters.ObjectFlags |= RF_Transient;
			Skeleton.Actor = World->SpawnActor<AActor>(ActorSpawnParameters);
			Skeleton.Actor->SetActorLabel(TEXT("PoseSearch"));
			Skeleton.Component = NewObject<UPoseSearchMeshComponent>(Skeleton.Actor.Get());
			Skeleton.Actor->AddInstanceComponent(Skeleton.Component.Get());
			Skeleton.Component->RegisterComponentWithWorld(World);
		}
		FWorldDelegates::OnWorldCleanup.AddRaw(this, &FDebuggerViewModel::OnWorldCleanup);
		bSkeletonsInitialized = true;
	}

	UpdateFromTimeline();
}

void FDebuggerViewModel::OnUpdateNodeSelection(int32 InNodeId)
{
	if (InNodeId == INDEX_NONE)
	{
		return;
	}

	ActiveMotionMatchingState = nullptr;

	// Find node in all motion matching states this frame
	const int32 NodesNum = NodeIds.Num();
	for (int i = 0; i < NodesNum; ++i)
	{
		if (NodeIds[i] == InNodeId)
		{
			ActiveMotionMatchingState = MotionMatchingStates[i];
			break;
		}
	}

	if (ActiveMotionMatchingState)
	{
		const UPoseSearchDatabase* CurrentDatabase = ActiveMotionMatchingState->GetCurrentDatabase();
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(CurrentDatabase, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& CurrentSearchIndex = CurrentDatabase->GetSearchIndex();
			int32 CurrentPoseIdx = ActiveMotionMatchingState->GetCurrentDatabasePoseIndex();
			if (const FSearchIndexAsset* IndexAsset = CurrentSearchIndex.GetAssetForPoseSafe(CurrentPoseIdx))
			{
				Skeletons[Asset].bMirrored = IndexAsset->bMirrored;
				Skeletons[Asset].SourceDatabase = CurrentDatabase;
				Skeletons[Asset].AssetIdx = IndexAsset->SourceAssetIdx;
				Skeletons[Asset].BlendParameters = IndexAsset->BlendParameters;
			}
		}
	}
}

void FDebuggerViewModel::UpdatePoseSearchContext(UPoseSearchMeshComponent::FUpdateContext& InOutContext, const FSkeleton& Skeleton) const
{
	const FInstancedStruct* DatabaseAssetStruct = Skeleton.GetAnimationAsset();
	if (DatabaseAssetStruct)
	{
		const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = DatabaseAssetStruct->GetPtr<FPoseSearchDatabaseAnimationAssetBase>();
		if (DatabaseAsset)
		{
			InOutContext.StartTime = Skeletons[SelectedPose].Time;
			InOutContext.Time = Skeletons[SelectedPose].Time;
			InOutContext.bMirrored = Skeletons[SelectedPose].bMirrored;
			InOutContext.bLoop = DatabaseAsset->IsLooping();
		}

		if (const FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAssetStruct->GetPtr<FPoseSearchDatabaseSequence>())
		{
			InOutContext.SequenceBase = DatabaseSequence->Sequence;
		}
		else if (const FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAssetStruct->GetPtr<FPoseSearchDatabaseAnimComposite>())
		{
			InOutContext.SequenceBase = DatabaseAnimComposite->AnimComposite;
		}
		else if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAssetStruct->GetPtr<FPoseSearchDatabaseBlendSpace>())
		{
			InOutContext.BlendSpace = DatabaseBlendSpace->BlendSpace;
			InOutContext.BlendParameters = Skeletons[SelectedPose].BlendParameters;
		}
		else
		{
			checkNoEntry();
		}
	}
}

void FDebuggerViewModel::UpdateFromTimeline()
{
	NodeIds.Empty();
	MotionMatchingStates.Empty();
	SkeletalMeshComponentId = 0;

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
	PoseSearchProvider->EnumerateMotionMatchingStateTimelines(AnimInstanceId, [&](const FTraceProvider::FMotionMatchingStateTimeline& InTimeline)
	{
		const FTraceMotionMatchingStateMessage* Message = nullptr;

		InTimeline.EnumerateEvents(Frame.StartTime, Frame.EndTime, [&Message](double InStartTime, double InEndTime, const FTraceMotionMatchingStateMessage& InMessage)
		{
			Message = &InMessage;
			return TraceServices::EEventEnumerate::Stop;
		});
		if (Message)
		{
			NodeIds.Add(Message->NodeId);
			MotionMatchingStates.Add(Message);
			SkeletalMeshComponentId = Message->SkeletalMeshComponentId;
		}
	});
	/** No active motion matching state as no messages were read */
	if (SkeletalMeshComponentId == 0)
	{
		return;
	}
	AnimationProvider->ReadSkeletalMeshPoseTimeline(SkeletalMeshComponentId, [&](const IAnimationProvider::SkeletalMeshPoseTimeline& TimelineData, bool bHasCurves)
	{
		TimelineData.EnumerateEvents(Frame.StartTime, Frame.EndTime, [&](double InStartTime, double InEndTime, uint32 InDepth, const FSkeletalMeshPoseMessage& PoseMessage) -> TraceServices::EEventEnumerate
		{
			const FSkeletalMeshInfo* SkeletalMeshInfo = AnimationProvider->FindSkeletalMeshInfo(PoseMessage.MeshId);
			const FObjectInfo* SkeletalMeshObjectInfo = GameplayProvider->FindObjectInfo(PoseMessage.MeshId);
			if (!SkeletalMeshInfo || !SkeletalMeshObjectInfo)
			{
				return TraceServices::EEventEnumerate::Stop;
			}

			UPoseSearchMeshComponent* ActiveComponent = Skeletons[ActivePose].Component.Get();
			UPoseSearchMeshComponent* SelectedComponent = Skeletons[SelectedPose].Component.Get();
			UPoseSearchMeshComponent* AssetComponent = Skeletons[Asset].Component.Get();
			USkeletalMesh* SkeletalMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(SkeletalMeshObjectInfo->PathName)).LoadSynchronous();
			if (SkeletalMesh)
			{
				ActiveComponent->SetSkinnedAssetAndUpdate(SkeletalMesh, true);
				SelectedComponent->SetSkinnedAssetAndUpdate(SkeletalMesh, true);
				AssetComponent->SetSkinnedAssetAndUpdate(SkeletalMesh, true);
			}
			FTransform ComponentWorldTransform;
			// Active skeleton is simply the traced bone transforms
			TArray<FTransform>& ComponentSpaceTransforms = ActiveComponent->GetEditableComponentSpaceTransforms();
			AnimationProvider->GetSkeletalMeshComponentSpacePose(PoseMessage, *SkeletalMeshInfo, ComponentWorldTransform, ComponentSpaceTransforms);

			check(ComponentWorldTransform.Equals(PoseMessage.ComponentToWorld));

			if (!ComponentSpaceTransforms.IsEmpty())
			{
				RootBoneWorldTransform = ComponentSpaceTransforms[RootBoneIndexType] * ComponentWorldTransform;
			}
			else
			{
				RootBoneWorldTransform = ComponentWorldTransform;
			}

			ActiveComponent->Initialize(ComponentWorldTransform);
			ActiveComponent->SetDebugDrawColor(FLinearColor::Green);
			SelectedComponent->SetDebugDrawColor(FLinearColor::Blue);
			SelectedComponent->Initialize(ComponentWorldTransform);
			AssetComponent->SetDebugDrawColor(FLinearColor::Red);
			AssetComponent->Initialize(ComponentWorldTransform);

			return TraceServices::EEventEnumerate::Stop;
		});
	});
}

const USkinnedMeshComponent* FDebuggerViewModel::GetMeshComponent() const
{
	if (Skeletons.Num() > FDebuggerViewModel::Asset)
	{
		return Skeletons[FDebuggerViewModel::Asset].Component.Get();
	}
	return nullptr;
}

void FDebuggerViewModel::FillCompactPoseAndComponentRefRotations()
{
	bool bResetMirrorBonesAndRotations = true;
	if (const UPoseSearchDatabase* Database = GetCurrentDatabase())
	{
		if (UMirrorDataTable* MirrorDataTable = Database->Schema->MirrorDataTable)
		{
			if (UPoseSearchMeshComponent* MeshComponent = Skeletons[ActivePose].Component.Get())
			{
				if (MeshComponent->RequiredBones.IsValid())
				{
					if (CompactPoseMirrorBones.Num() == 0 || ComponentSpaceRefRotations.Num() == 0)
					{
						MirrorDataTable->FillCompactPoseAndComponentRefRotations(MeshComponent->RequiredBones, CompactPoseMirrorBones, ComponentSpaceRefRotations);
					}
					bResetMirrorBonesAndRotations = false;
				}
			}
		}
	}

	if (bResetMirrorBonesAndRotations)
	{
		CompactPoseMirrorBones.Reset();
		ComponentSpaceRefRotations.Reset();
	}
}

void FDebuggerViewModel::OnWorldCleanup(UWorld* InWorld, bool bSessionEnded, bool bCleanupResources)
{
	bSkeletonsInitialized = false;
}

const FInstancedStruct* FDebuggerViewModel::FSkeleton::GetAnimationAsset() const
{
	if (SourceDatabase.IsValid() && SourceDatabase->AnimationAssets.IsValidIndex(AssetIdx))
	{
		return &SourceDatabase->GetAnimationAssetStruct(AssetIdx);
	}
	return nullptr;
}

} // namespace UE::PoseSearch
