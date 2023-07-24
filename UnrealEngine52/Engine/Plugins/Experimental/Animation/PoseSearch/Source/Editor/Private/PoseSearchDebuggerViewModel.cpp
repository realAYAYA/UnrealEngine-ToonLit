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
		if (Skeleton.Actor.IsValid())
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

const UPoseSearchSearchableAsset* FDebuggerViewModel::GetSearchableAsset() const
{
	if (ActiveMotionMatchingState)
	{
		return FTraceMotionMatchingState::GetObjectFromId<UPoseSearchSearchableAsset>(ActiveMotionMatchingState->SearchableAssetId);
	}

	return nullptr;
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

	const FPoseSearchIndex& SearchIndex = Database->GetSearchIndex();
	const FPoseSearchIndexAsset& IndexAsset = SearchIndex.GetAssetForPose(DbPoseIdx);
	
	Component->ResetToStart(); 
	bSelecting = true;
	
	Skeletons[SelectedPose].Type = IndexAsset.Type;
	Skeletons[SelectedPose].Time = Time;
	Skeletons[SelectedPose].bMirrored = IndexAsset.bMirrored;
	Skeletons[SelectedPose].SourceDatabase = Database;
	Skeletons[SelectedPose].AssetIdx = IndexAsset.SourceAssetIdx;
	Skeletons[SelectedPose].BlendParameters = IndexAsset.BlendParameters;
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

const FTransform* FDebuggerViewModel::GetRootTransform() const
{
	return RootTransform;
}

bool FDebuggerViewModel::HasSearchableAssetChanged() const
{
	uint64 NewSearchableAssetId = ActiveMotionMatchingState ? ActiveMotionMatchingState->SearchableAssetId : 0;
	return NewSearchableAssetId != SearchableAssetId;
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
			const FPoseSearchIndex& CurrentSearchIndex = CurrentDatabase->GetSearchIndex();
			int32 CurrentPoseIdx = ActiveMotionMatchingState->GetCurrentDatabasePoseIndex();
			if (const FPoseSearchIndexAsset* IndexAsset = CurrentSearchIndex.GetAssetForPoseSafe(CurrentPoseIdx))
			{
				Skeletons[Asset].Type = IndexAsset->Type;
				Skeletons[Asset].bMirrored = IndexAsset->bMirrored;
				Skeletons[Asset].SourceDatabase = CurrentDatabase;
				Skeletons[Asset].AssetIdx = IndexAsset->SourceAssetIdx;
				Skeletons[Asset].BlendParameters = IndexAsset->BlendParameters;
			}
		}
	}

	uint64 NewSearchableAssetId = ActiveMotionMatchingState ? ActiveMotionMatchingState->SearchableAssetId : 0;
	if (NewSearchableAssetId != SearchableAssetId)
	{
		ClearSelectedSkeleton();
		SearchableAssetId = NewSearchableAssetId;
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
			InOutContext.Type = DatabaseAsset->GetSearchIndexType();
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

void FDebuggerViewModel::OnDraw(FSkeletonDrawParams& DrawParams)
{
	const UPoseSearchDatabase* CurrentDatabase = GetCurrentDatabase();
	if (!CurrentDatabase)
	{
		return;
	}

	// Returns if it is to be drawn this frame
	auto SetDrawSkeleton = [this](UPoseSearchMeshComponent* InComponent, bool bDraw)
	{
		if (InComponent && InComponent->RequiredBones.IsValid())
		{
			const bool bIsDrawingSkeleton = InComponent->ShouldDrawDebugSkeleton();
			if (bIsDrawingSkeleton != bDraw)
			{
				InComponent->SetDrawDebugSkeleton(bDraw);
			}
			InComponent->MarkRenderStateDirty();
		}
	};
	const bool bDrawActivePose = EnumHasAnyFlags(DrawParams.Flags, ESkeletonDrawFlags::ActivePose);
	SetDrawSkeleton(Skeletons[ActivePose].Component.Get(), bDrawActivePose);
	// If flag is set and we are currently in a valid drawing state
	const bool bDrawSelectedPose = EnumHasAnyFlags(DrawParams.Flags, ESkeletonDrawFlags::SelectedPose) && bSelecting;
	SetDrawSkeleton(Skeletons[SelectedPose].Component.Get(), bDrawSelectedPose);

	FillCompactPoseAndComponentRefRotations();

	UPoseSearchMeshComponent::FUpdateContext UpdateContext;

	UpdateContext.MirrorDataTable = CurrentDatabase->Schema->MirrorDataTable;
	UpdateContext.CompactPoseMirrorBones = &CompactPoseMirrorBones;
	UpdateContext.ComponentSpaceRefRotations = &ComponentSpaceRefRotations;

	if (bDrawSelectedPose)
	{
		UPoseSearchMeshComponent* Component = Skeletons[SelectedPose].Component.Get();
		if (Component && Component->RequiredBones.IsValid())
		{
			UpdatePoseSearchContext(UpdateContext, Skeletons[SelectedPose]);

			if (UpdateContext.Type != ESearchIndexAssetType::Invalid)
			{
				Component->UpdatePose(UpdateContext);
			}
		}
	}

	const bool bDrawAsset = EnumHasAnyFlags(DrawParams.Flags, ESkeletonDrawFlags::Asset);
	if (bDrawAsset && AssetData.bActive)
	{
		UPoseSearchMeshComponent* Component = Skeletons[Asset].Component.Get();
		if (Component && Component->RequiredBones.IsValid())
		{
			SetDrawSkeleton(Component, true);

			UpdatePoseSearchContext(UpdateContext, Skeletons[SelectedPose]);

			if (UpdateContext.Type != ESearchIndexAssetType::Invalid)
			{
				Component->UpdatePose(UpdateContext);
			}
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
			// Update root transform
			RootTransform = &PoseMessage.ComponentToWorld;
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
			AnimationProvider->GetSkeletalMeshComponentSpacePose(PoseMessage, *SkeletalMeshInfo, ComponentWorldTransform, ActiveComponent->GetEditableComponentSpaceTransforms());
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

void FDebuggerViewModel::UpdateAsset()
{
	// @todo: expose those parameters
	static float MAX_DISTANCE_RANGE = 200.f;
	static float MAX_TIME_RANGE = 2.f;

	const UPoseSearchDatabase* Database = GetCurrentDatabase();
	if (!Database || !IsPlayingSelections())
	{
		return;
	}

	FSkeleton& AssetSkeleton = Skeletons[Asset];
	UPoseSearchMeshComponent* Component = AssetSkeleton.Component.Get();

	auto RestartAsset = [&]()
	{
		Component->ResetToStart();
		AssetData.AccumulatedTime = 0.0;
		AssetSkeleton.Time = AssetData.StartTime;
	};

	const FInstancedStruct* AnimationAssetStruct = AssetSkeleton.GetAnimationAsset();
	if (!AnimationAssetStruct)
	{
		checkNoEntry();
		return;
	}

	const FPoseSearchDatabaseAnimationAssetBase* DatabaseAsset = AnimationAssetStruct->GetPtr<FPoseSearchDatabaseAnimationAssetBase>();
	if (!DatabaseAsset)
	{
		checkNoEntry();
		return;
	}

	const UAnimationAsset* AnimAsset = DatabaseAsset->GetAnimationAsset();
	const bool bAssetLooping = DatabaseAsset->IsLooping();

	const float DT = static_cast<float>(FApp::GetDeltaTime()) * AssetPlayRate;
	const float PlayLength = AnimAsset->GetPlayLength();
	const bool bExceededDistanceHorizon = Component->LastRootMotionDelta.GetTranslation().Size() > MAX_DISTANCE_RANGE;
	const bool bExceededTimeHorizon = (AssetSkeleton.Time - AssetData.StartTime) > MAX_TIME_RANGE;
	const bool bExceededHorizon = bExceededDistanceHorizon && bExceededTimeHorizon;
	if (bAssetLooping)
	{
		if (bExceededHorizon)
		{
			// Delay before restarting the asset to give the user some idea of where it would land
			if (AssetData.AccumulatedTime > AssetData.StopDuration)
			{
				RestartAsset();
			}
			else
			{
				AssetData.AccumulatedTime += DT;
			}
			return;
		}

		AssetSkeleton.Time += DT;
		AssetData.AccumulatedTime += DT;
	}
	else
	{
		// Used to cap the asset, but avoid modding when updating the pose
		static constexpr float LengthOffset = 0.001f;
		const bool bFinishedAsset = AssetSkeleton.Time >= PlayLength - LengthOffset;

		// Asset player reached end of clip or reached distance horizon of trajectory vector
		if (bFinishedAsset || bExceededHorizon)
		{
			// Delay before restarting the asset to give the user some idea of where it would land
			if (AssetData.AccumulatedTime > AssetData.StopDuration)
			{
				RestartAsset();
			}
			else
			{
				AssetData.AccumulatedTime += DT;
			}
		}
		else
		{
			// If we haven't finished, update the play time capped by the anim asset (not looping)
			AssetSkeleton.Time += DT;
		}
	}
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

void FDebuggerViewModel::PlaySelection(int32 PoseIdx, float Time)
{
	UPoseSearchMeshComponent* Component = Skeletons[Asset].Component.Get();
	if (!Component)
	{
		return;
	}

	const UPoseSearchDatabase* Database = GetCurrentDatabase();
	if (!FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(Database, ERequestAsyncBuildFlag::ContinueRequest))
	{
		return;
	}
	
	const FPoseSearchIndexAsset& IndexAsset = Database->GetSearchIndex().GetAssetForPose(PoseIdx);
	Component->ResetToStart();
	
	Skeletons[Asset].Type = IndexAsset.Type;
	Skeletons[Asset].AssetIdx = IndexAsset.SourceAssetIdx;
	Skeletons[Asset].Time = Time;
	Skeletons[Asset].bMirrored = IndexAsset.bMirrored;
	Skeletons[Asset].BlendParameters = IndexAsset.BlendParameters;

	AssetData.StartTime = Time;
	AssetData.AccumulatedTime = 0.0f;
	AssetData.bActive = true;
}

void FDebuggerViewModel::StopSelection()
{
	UPoseSearchMeshComponent* Component = Skeletons[Asset].Component.Get();
	if (!Component)
	{
		return;
	}

	AssetData = {};
	// @TODO: Make more functionality rely on checking if it should draw the asset
	Component->SetDrawDebugSkeleton(false);
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
