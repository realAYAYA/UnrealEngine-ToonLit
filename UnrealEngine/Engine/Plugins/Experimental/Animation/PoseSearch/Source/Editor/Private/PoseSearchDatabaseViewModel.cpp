// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearchDatabasePreviewScene.h"
#include "SPoseSearchDatabaseAssetList.h"
#include "PoseSearchEditor.h"
#include "PoseSearch/PoseSearch.h"
#include "Modules/ModuleManager.h"
#include "AnimPreviewInstance.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "EngineUtils.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"


namespace UE::PoseSearch
{
	constexpr float StepDeltaTime = 1.0f / 30.0f;

	FDatabaseViewModel::FDatabaseViewModel()
		: PoseSearchDatabase(nullptr)
	{
	}

	FDatabaseViewModel::~FDatabaseViewModel()
	{
		PoseSearchDatabase->UnregisterOnDerivedDataRebuild(this);
	}

	void FDatabaseViewModel::AddReferencedObjects(FReferenceCollector& Collector)
	{
		Collector.AddReferencedObject(PoseSearchDatabase);
	}

	void FDatabaseViewModel::Initialize(
		UPoseSearchDatabase* InPoseSearchDatabase,
		const TSharedRef<FDatabasePreviewScene>& InPreviewScene)
	{
		PoseSearchDatabase = InPoseSearchDatabase;
		PreviewScenePtr = InPreviewScene;

		RemovePreviewActors();

		PoseSearchDatabase->RegisterOnDerivedDataRebuild(
			UPoseSearchDatabase::FOnDerivedDataRebuild::CreateSP(
				this,
				&FDatabaseViewModel::ResetPreviewActors));
	}

	void FDatabaseViewModel::ResetPreviewActors()
	{
		PlayTime = 0.0f;
		DeltaTimeMultiplier = 1.0f;
		RespawnPreviewActors();
	}

	void FDatabaseViewModel::RespawnPreviewActors()
	{
		RemovePreviewActors();

		MaxPreviewPlayLength = 0.0f;

		if (AnimationPreviewMode == EAnimationPreviewMode::None && SelectedNodes.IsEmpty())
		{
			return;
		}

		TSet<int32> AssociatedSequencesAssetIndices;
		TSet<int32> AssociatedBlendSpacesAssetIndices;
		for (const TSharedPtr<FDatabaseAssetTreeNode>& SelectedNode : SelectedNodes)
		{
			if (SelectedNode->SourceAssetType == ESearchIndexAssetType::Sequence)
			{
				AssociatedSequencesAssetIndices.Add(SelectedNode->SourceAssetIdx);
			}
			else if (SelectedNode->SourceAssetType == ESearchIndexAssetType::BlendSpace)
			{
				AssociatedBlendSpacesAssetIndices.Add(SelectedNode->SourceAssetIdx);
			}
		}

		const FPoseSearchIndex* SearchIndex = PoseSearchDatabase->GetSearchIndexSafe();
		if (SearchIndex)
		{
			for (const FPoseSearchIndexAsset& IndexAsset : SearchIndex->Assets)
			{
				bool bSpawn = false;
				if (AnimationPreviewMode == EAnimationPreviewMode::OriginalAndMirrored || !IndexAsset.bMirrored)
				{
					bool bIsAssociatedToSelection = false;
					if (IndexAsset.Type == ESearchIndexAssetType::Sequence)
					{
						bSpawn = AssociatedSequencesAssetIndices.Contains(IndexAsset.SourceAssetIdx);
					}
					else if (IndexAsset.Type == ESearchIndexAssetType::BlendSpace)
					{
						bSpawn = AssociatedBlendSpacesAssetIndices.Contains(IndexAsset.SourceAssetIdx);
					}
				}

				if (bSpawn)
				{
					FDatabasePreviewActor PreviewActor = SpawnPreviewActor(IndexAsset);
					if (PreviewActor.IsValid())
					{
						const UAnimationAsset* AnimationAsset = PreviewActor.AnimInstance->GetAnimationAsset();
						if (AnimationAsset)
						{
							MaxPreviewPlayLength = FMath::Max(MaxPreviewPlayLength, AnimationAsset->GetPlayLength());
						}
						
						PreviewActors.Add(PreviewActor);
					}
				}
			}

			UpdatePreviewActors();
		}
	}

	void FDatabaseViewModel::BuildSearchIndex()
	{
		PoseSearchDatabase->NotifyAssetChange();
		PoseSearchDatabase->BeginCacheDerivedData();
	}

	void FDatabaseViewModel::PreviewBackwardEnd()
	{
		PlayTime = 0.0f;
	}

	void FDatabaseViewModel::PreviewBackwardStep()
	{
		PlayTime = FMath::Clamp(PlayTime - StepDeltaTime, 0.0f, MaxPreviewPlayLength);
		DeltaTimeMultiplier = 0.0f;
	}

	void FDatabaseViewModel::PreviewBackward()
	{
		DeltaTimeMultiplier = -1.0f;
	}

	void FDatabaseViewModel::PreviewPause()
	{
		DeltaTimeMultiplier = 0.0f;
	}

	void FDatabaseViewModel::PreviewForward()
	{
		DeltaTimeMultiplier = 1.0f;
	}

	void FDatabaseViewModel::PreviewForwardStep()
	{
		PlayTime = FMath::Clamp(PlayTime + StepDeltaTime, 0.0f, MaxPreviewPlayLength);
		DeltaTimeMultiplier = 0.0f;
	}

	void FDatabaseViewModel::PreviewForwardEnd()
	{
		PlayTime = MaxPreviewPlayLength;
	}

	FDatabasePreviewActor FDatabaseViewModel::SpawnPreviewActor(const FPoseSearchIndexAsset& IndexAsset)
	{
		FDatabasePreviewActor PreviewActor;

		if (!PoseSearchDatabase || !PoseSearchDatabase->Schema || !PoseSearchDatabase->Schema->Skeleton)
		{
			return PreviewActor;
		}

		UAnimationAsset* PreviewAsset = nullptr;
		if (IndexAsset.Type == ESearchIndexAssetType::Sequence)
		{
			FPoseSearchDatabaseSequence DatabaseSequence = PoseSearchDatabase->Sequences[IndexAsset.SourceAssetIdx];
			PreviewAsset = DatabaseSequence.Sequence;
		}
		else if (IndexAsset.Type == ESearchIndexAssetType::BlendSpace)
		{
			FPoseSearchDatabaseBlendSpace DatabaseBlendSpace = PoseSearchDatabase->BlendSpaces[IndexAsset.SourceAssetIdx];
			PreviewAsset = DatabaseBlendSpace.BlendSpace;
		}

		if (!PreviewAsset)
		{
			return PreviewActor;
		}

		PreviewActor.IndexAsset = IndexAsset;
		PreviewActor.CurrentPoseIndex = INDEX_NONE;

		// todo: use preview when possible
		UClass* PreviewClass = AActor::StaticClass();
		const FTransform SpawnTransform = FTransform::Identity;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		PreviewActor.Actor = GetWorld()->SpawnActor<AActor>(PreviewClass, SpawnTransform, Params);
		PreviewActor.Actor->SetFlags(RF_Transient);

		PreviewActor.Mesh = NewObject<UDebugSkelMeshComponent>(PreviewActor.Actor.Get());
		PreviewActor.Mesh->RegisterComponentWithWorld(GetWorld());

		PreviewActor.AnimInstance = NewObject<UAnimPreviewInstance>(PreviewActor.Mesh.Get());
		PreviewActor.Mesh->PreviewInstance = PreviewActor.AnimInstance.Get();
		PreviewActor.AnimInstance->InitializeAnimation();
		PreviewActor.AnimInstance->SetBlendSpacePosition(IndexAsset.BlendParameters);

		PreviewActor.Mesh->SetSkeletalMesh(PoseSearchDatabase->Schema->Skeleton->GetPreviewMesh(true));
		PreviewActor.Mesh->EnablePreview(true, PreviewAsset);
		PreviewActor.AnimInstance->SetAnimationAsset(PreviewAsset, false, 0.0f);
		if (IndexAsset.bMirrored && PoseSearchDatabase->Schema)
		{
			PreviewActor.AnimInstance->SetMirrorDataTable(PoseSearchDatabase->Schema->MirrorDataTable);
		}

		PreviewActor.AnimInstance->PlayAnim(false, 0.0f);

		if (!PreviewActor.Actor->GetRootComponent())
		{
			PreviewActor.Actor->SetRootComponent(PreviewActor.Mesh.Get());
		}

		UE_LOG(
			LogPoseSearchEditor, Log, TEXT("Spawned preview Actor: %s at Loc: %s Rot: %s"),
			*GetNameSafe(PreviewActor.Actor.Get()),
			*SpawnTransform.GetLocation().ToString(),
			*SpawnTransform.Rotator().ToString());

		return PreviewActor;
	}

	UWorld* FDatabaseViewModel::GetWorld() const
	{
		check(PreviewScenePtr.IsValid());
		return PreviewScenePtr.Pin()->GetWorld();
	}

	UObject* FDatabaseViewModel::GetPlaybackContext() const
	{
		return GetWorld();
	}


	void FDatabaseViewModel::OnPreviewActorClassChanged()
	{
		// todo: implement
	}

	void FDatabaseViewModel::Tick(float DeltaSeconds)
	{
		PlayTime += DeltaSeconds * DeltaTimeMultiplier;
		PlayTime = FMath::Clamp(PlayTime, 0.0f, MaxPreviewPlayLength);
		UpdatePreviewActors();
	}

	void FDatabaseViewModel::UpdatePreviewActors()
	{
		TSharedPtr<FDatabasePreviewScene> PreviewScene = PreviewScenePtr.Pin();
		for (FDatabasePreviewActor& PreviewActor : GetPreviewActors())
		{
			if (const UAnimationAsset* PreviewAsset = PreviewActor.AnimInstance->GetAnimationAsset())
			{
				float CurrentTime = 0.0f;
				FAnimationRuntime::AdvanceTime(false, PlayTime, CurrentTime, PreviewAsset->GetPlayLength());

				if (const UAnimSequence* AnimSequence = Cast<UAnimSequence>(PreviewAsset))
				{
					FTransform RootMotion = FTransform::Identity;
					RootMotion = AnimSequence->ExtractRootMotionFromRange(0.0f, CurrentTime);
					if (PreviewActor.AnimInstance->GetMirrorDataTable())
					{
						RootMotion = MirrorRootMotion(RootMotion, PreviewActor.AnimInstance->GetMirrorDataTable());
					}
					PreviewActor.Actor->SetActorTransform(RootMotion);
				}
				else if (const UBlendSpace* BlendSpace = Cast<UBlendSpace>(PreviewAsset))
				{
					// @todo: add BS support
					//RootMotion = BlendSpace->ExtractRootMotionFromRange(0.0f, CurrentTime);
				}
				
				PreviewActor.AnimInstance->SetPosition(CurrentTime);

				PreviewActor.CurrentPoseIndex = PoseSearchDatabase->GetPoseIndexFromTime(CurrentTime, &PreviewActor.IndexAsset);
			}
		}
	}

	void FDatabaseViewModel::RemovePreviewActors()
	{
		for (auto PreviewActor : PreviewActors)
		{
			PreviewActor.Actor->Destroy();
		}
		PreviewActors.Reset();

		MaxPreviewPlayLength = 0.0f;
	}

	FTransform FDatabaseViewModel::MirrorRootMotion(
		FTransform RootMotion,
		const UMirrorDataTable* MirrorDataTable)
	{
		const FTransform RootReferenceTransform =
			PoseSearchDatabase->Schema->Skeleton->GetReferenceSkeleton().GetRefBonePose()[0];
		const FQuat RootReferenceRotation = RootReferenceTransform.GetRotation();

		const EAxis::Type MirrorAxis = MirrorDataTable->MirrorAxis;
		FVector T = RootMotion.GetTranslation();
		T = FAnimationRuntime::MirrorVector(T, MirrorAxis);
		FQuat Q = RootMotion.GetRotation();
		Q = FAnimationRuntime::MirrorQuat(Q, MirrorAxis);
		Q *= FAnimationRuntime::MirrorQuat(RootReferenceRotation, MirrorAxis).Inverse() * RootReferenceRotation;

		FTransform MirroredRootMotion = FTransform(Q, T, RootMotion.GetScale3D());
		return MirroredRootMotion;
	}

	void FDatabaseViewModel::OnSetPoseFeaturesDrawMode(EFeaturesDrawMode DrawMode)
	{
		PoseFeaturesDrawMode = DrawMode;
	}

	bool FDatabaseViewModel::IsPoseFeaturesDrawMode(EFeaturesDrawMode DrawMode) const
	{
		return PoseFeaturesDrawMode == DrawMode;
	}

	void FDatabaseViewModel::OnSetAnimationPreviewMode(EAnimationPreviewMode PreviewMode)
	{
		if (PreviewMode != AnimationPreviewMode)
		{
			AnimationPreviewMode = PreviewMode;
			RespawnPreviewActors();
		}
	}

	bool FDatabaseViewModel::IsAnimationPreviewMode(EAnimationPreviewMode PreviewMode) const
	{
		return AnimationPreviewMode == PreviewMode;
	}

	void FDatabaseViewModel::AddSequenceToDatabase(UAnimSequence* AnimSequence)
	{
		FPoseSearchDatabaseSequence& NewDbSequence = PoseSearchDatabase->Sequences.AddDefaulted_GetRef();
		NewDbSequence.Sequence = AnimSequence;
	}

	void FDatabaseViewModel::AddBlendSpaceToDatabase(UBlendSpace* BlendSpace)
	{
		FPoseSearchDatabaseBlendSpace& NewDbBlendSpace = PoseSearchDatabase->BlendSpaces.AddDefaulted_GetRef();
		NewDbBlendSpace.BlendSpace = BlendSpace;
	}

	void FDatabaseViewModel::DeleteSequenceFromDatabase(int32 SequenceIdx)
	{
		PoseSearchDatabase->Sequences.RemoveAt(SequenceIdx);
	}

	void FDatabaseViewModel::DeleteBlendSpaceFromDatabase(int32 BlendSpaceIdx)
	{
		PoseSearchDatabase->BlendSpaces.RemoveAt(BlendSpaceIdx);
	}

	void FDatabaseViewModel::SetSelectedSequenceEnabled(int32 SequenceIndex, bool bEnabled)
	{
		PoseSearchDatabase->Sequences[SequenceIndex].bEnabled = bEnabled;
	}	
	
	void FDatabaseViewModel::SetSelectedBlendSpaceEnabled(int32 BlendSpaceIndex, bool bEnabled)
	{
		PoseSearchDatabase->BlendSpaces[BlendSpaceIndex].bEnabled = bEnabled;
	}

	bool FDatabaseViewModel::IsSelectedSequenceEnabled(int32 SequenceIndex) const
	{
		return PoseSearchDatabase->Sequences[SequenceIndex].bEnabled;
	}

	bool FDatabaseViewModel::IsSelectedBlendSpaceEnabled(int32 BlendSpaceIndex) const
	{
		return PoseSearchDatabase->BlendSpaces[BlendSpaceIndex].bEnabled;
	}

	void FDatabaseViewModel::SetSelectedNodes(const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& InSelectedNodes)
	{
		SelectedNodes = InSelectedNodes;
		ResetPreviewActors();
		ProcessSelectedActor(nullptr);
	}

	void FDatabaseViewModel::ProcessSelectedActor(AActor* Actor)
	{
		SelectedActorIndexAsset = nullptr;
		
		const FDatabasePreviewActor* SelectedPreviewActor = PreviewActors.FindByPredicate(
			[Actor](const FDatabasePreviewActor& PreviewActor)
		{
			return PreviewActor.Actor == Actor;
		});

		if (SelectedPreviewActor)
		{
			SelectedActorIndexAsset = &SelectedPreviewActor->IndexAsset;
		}
	}

	float FDatabaseViewModel::GetMaxPreviewPlayLength() const
	{
		return MaxPreviewPlayLength;
	}

	float FDatabaseViewModel::GetPlayTime() const
	{
		const float ClampedPlayTime = FMath::Clamp(PlayTime, 0.0f, MaxPreviewPlayLength);
		return ClampedPlayTime;
	}

	void FDatabaseViewModel::SetPlayTime(float NewPlayTime, bool bInTickPlayTime)
	{
		PlayTime = NewPlayTime;
		DeltaTimeMultiplier = bInTickPlayTime ? DeltaTimeMultiplier : 0.0f;
	}
}
