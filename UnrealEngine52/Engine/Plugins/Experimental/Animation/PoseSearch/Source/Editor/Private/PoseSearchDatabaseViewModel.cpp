// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseViewModel.h"
#include "PoseSearchDatabasePreviewScene.h"
#include "PoseSearchDatabaseAssetTreeNode.h"
#include "PoseSearchEditor.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "Modules/ModuleManager.h"
#include "AnimPreviewInstance.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimSequence.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/MirrorDataTable.h"
#include "Animation/BlendSpace.h"
#include "EngineUtils.h"
#include "InstancedStruct.h"
#include "PropertyEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/PlayerController.h"

namespace UE::PoseSearch
{
	constexpr float StepDeltaTime = 1.0f / 30.0f;

	// FDatabasePreviewActor
	bool FDatabasePreviewActor::IsValid() const
	{
		return Actor != nullptr;
	}

	void FDatabasePreviewActor::Process()
	{
		switch (Type)
		{
		case ESearchIndexAssetType::Sequence:
		{
			SequenceSampler.Process();
			break;
		}
		case ESearchIndexAssetType::BlendSpace:
		{
			BlendSpaceSampler.Process();
			break;
		}
		case ESearchIndexAssetType::AnimComposite:
		{
			SequenceSampler.Process();
			break;
		}
		default:
		{
			checkNoEntry();
			break;
		}
		}
	}

	const IAssetSampler* FDatabasePreviewActor::GetSampler() const
	{
		switch (Type)
		{
		case ESearchIndexAssetType::Sequence:
			return &SequenceSampler;
		case ESearchIndexAssetType::BlendSpace:
			return &BlendSpaceSampler;
		case ESearchIndexAssetType::AnimComposite:
			return &SequenceSampler;
		}

		checkNoEntry();
		return nullptr;
	}

	float FDatabasePreviewActor::GetScaledTime(float Time) const
	{
		float ScaledTime = Time;
		if (Type == ESearchIndexAssetType::BlendSpace)
		{
			ScaledTime = BlendSpaceSampler.GetPlayLength() > UE_KINDA_SMALL_NUMBER ? Time / BlendSpaceSampler.GetPlayLength() : 0.f;
		}
		return ScaledTime;
	}

	UDebugSkelMeshComponent* FDatabasePreviewActor::GetDebugSkelMeshComponent()
	{
		if (IsValid())
		{
			return Cast<UDebugSkelMeshComponent>(Actor->GetRootComponent());
		}
		return nullptr;
	}

	UAnimPreviewInstance* FDatabasePreviewActor::GetAnimPreviewInstance()
	{
		if (UDebugSkelMeshComponent* Mesh = GetDebugSkelMeshComponent())
		{
			return Mesh->PreviewInstance.Get();
		}
		return nullptr;
	}

	// FDatabaseViewModel
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

	void FDatabaseViewModel::Initialize(UPoseSearchDatabase* InPoseSearchDatabase, const TSharedRef<FDatabasePreviewScene>& InPreviewScene)
	{
		PoseSearchDatabase = InPoseSearchDatabase;
		PreviewScenePtr = InPreviewScene;

		RemovePreviewActors();

		PoseSearchDatabase->RegisterOnDerivedDataRebuild(UPoseSearchDatabase::FOnDerivedDataRebuild::CreateSP(this, &FDatabaseViewModel::ResetPreviewActors));
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
			if (SelectedNode->SourceAssetType == ESearchIndexAssetType::Sequence || SelectedNode->SourceAssetType == ESearchIndexAssetType::AnimComposite)
			{
				AssociatedSequencesAssetIndices.Add(SelectedNode->SourceAssetIdx);
			}
			else if (SelectedNode->SourceAssetType == ESearchIndexAssetType::BlendSpace)
			{
				AssociatedBlendSpacesAssetIndices.Add(SelectedNode->SourceAssetIdx);
			}
		}

		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
		{
			FBoneContainer BoneContainer;
			BoneContainer.InitializeTo(PoseSearchDatabase->Schema->BoneIndicesWithParents, FCurveEvaluationOption(false), *PoseSearchDatabase->Schema->Skeleton);

			const FPoseSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
			for (int32 IndexAssetIndex = 0; IndexAssetIndex < SearchIndex.Assets.Num(); ++IndexAssetIndex)
			{
				const FPoseSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];

				bool bSpawn = false;
				if (AnimationPreviewMode == EAnimationPreviewMode::OriginalAndMirrored || !IndexAsset.bMirrored)
				{
					bool bIsAssociatedToSelection = false;
					if (IndexAsset.Type == ESearchIndexAssetType::Sequence || IndexAsset.Type == ESearchIndexAssetType::AnimComposite)
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
					FDatabasePreviewActor PreviewActor = SpawnPreviewActor(IndexAssetIndex, BoneContainer);
					if (PreviewActor.IsValid())
					{
						const UAnimationAsset* AnimationAsset = PreviewActor.GetAnimPreviewInstance()->GetAnimationAsset();
						if (AnimationAsset)
						{
							MaxPreviewPlayLength = FMath::Max(MaxPreviewPlayLength, AnimationAsset->GetPlayLength());
						}
						
						PreviewActors.Add(PreviewActor);
					}
				}
			}

			ParallelFor(PreviewActors.Num(), [this](int32 PreviewActorIndex) { PreviewActors[PreviewActorIndex].Process(); }, ParallelForFlags);

			UpdatePreviewActors();
		}
	}

	void FDatabaseViewModel::BuildSearchIndex()
	{
		using namespace UE::PoseSearch;
		FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::NewRequest);
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

	FDatabasePreviewActor FDatabaseViewModel::SpawnPreviewActor(int32 IndexAssetIndex, const FBoneContainer& BoneContainer)
	{
		FDatabasePreviewActor PreviewActor;

		check(PoseSearchDatabase);
		const FPoseSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
		const FPoseSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];

		const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = PoseSearchDatabase->GetAnimationAssetBase(IndexAsset.SourceAssetIdx);
		UAnimationAsset* PreviewAsset = DatabaseAnimationAsset->GetAnimationAsset();
		if (!PreviewAsset)
		{
			return PreviewActor;
		}

		const FInstancedStruct& DatabaseAsset = PoseSearchDatabase->GetAnimationAssetStruct(IndexAsset);
		if (const FPoseSearchDatabaseSequence* DatabaseSequence = DatabaseAsset.GetPtr<FPoseSearchDatabaseSequence>())
		{
			FSequenceBaseSampler::FInput Input;
			Input.ExtrapolationParameters = PoseSearchDatabase->ExtrapolationParameters;
			Input.SequenceBase = DatabaseSequence->Sequence;

			PreviewActor.SequenceSampler.Init(Input);
		}
		else if (const FPoseSearchDatabaseAnimComposite* DatabaseAnimComposite = DatabaseAsset.GetPtr<FPoseSearchDatabaseAnimComposite>())
		{
			FSequenceBaseSampler::FInput Input;
			Input.ExtrapolationParameters = PoseSearchDatabase->ExtrapolationParameters;
			Input.SequenceBase = DatabaseAnimComposite->AnimComposite;

			PreviewActor.SequenceSampler.Init(Input);
		}
		else if (const FPoseSearchDatabaseBlendSpace* DatabaseBlendSpace = DatabaseAsset.GetPtr<FPoseSearchDatabaseBlendSpace>())
		{
			FBlendSpaceSampler::FInput Input;
			Input.BoneContainer = BoneContainer;
			Input.ExtrapolationParameters = PoseSearchDatabase->ExtrapolationParameters;
			Input.BlendSpace = DatabaseBlendSpace->BlendSpace;
			Input.BlendParameters = IndexAsset.BlendParameters;

			PreviewActor.BlendSpaceSampler.Init(Input);
		}
		else
		{
			checkNoEntry();
		}

		PreviewActor.Type = IndexAsset.Type;
		PreviewActor.IndexAssetIndex = IndexAssetIndex;
		PreviewActor.CurrentPoseIndex = INDEX_NONE;

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		PreviewActor.Actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		PreviewActor.Actor->SetFlags(RF_Transient);

		UDebugSkelMeshComponent* Mesh = NewObject<UDebugSkelMeshComponent>(PreviewActor.Actor.Get());
		Mesh->RegisterComponentWithWorld(GetWorld());

		UAnimPreviewInstance* AnimInstance = NewObject<UAnimPreviewInstance>(Mesh);

		Mesh->PreviewInstance = AnimInstance;
		AnimInstance->InitializeAnimation();

		Mesh->SetSkeletalMesh(PoseSearchDatabase->Schema->Skeleton->GetPreviewMesh(true));
		Mesh->EnablePreview(true, PreviewAsset);
		
		AnimInstance->SetAnimationAsset(PreviewAsset, false, 0.0f);
		AnimInstance->SetBlendSpacePosition(IndexAsset.BlendParameters);
		
		if (IndexAsset.bMirrored && PoseSearchDatabase->Schema)
		{
			AnimInstance->SetMirrorDataTable(PoseSearchDatabase->Schema->MirrorDataTable);
		}

		AnimInstance->PlayAnim(false, 0.0f);

		if (!PreviewActor.Actor->GetRootComponent())
		{
			PreviewActor.Actor->SetRootComponent(Mesh);
		}

		AnimInstance->SetPlayRate(0.f);

		UE_LOG(LogPoseSearchEditor, Log, TEXT("Spawned preview Actor: %s"), *GetNameSafe(PreviewActor.Actor.Get()));
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

		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
		{
			UpdatePreviewActors(true);
		}
	}

	void FDatabaseViewModel::UpdatePreviewActors(bool bInTickPlayTime)
	{
		const FPoseSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
		TSharedPtr<FDatabasePreviewScene> PreviewScene = PreviewScenePtr.Pin();
		for (FDatabasePreviewActor& PreviewActor : GetPreviewActors())
		{
			UAnimPreviewInstance* AnimInstance = PreviewActor.GetAnimPreviewInstance();
			if (!AnimInstance || PreviewActor.IndexAssetIndex >= SearchIndex.Assets.Num())
			{
				continue;
			}

			const UAnimationAsset* PreviewAsset = AnimInstance->GetAnimationAsset();
			const IAssetSampler* Sampler = PreviewActor.GetSampler();
			if (!PreviewAsset || !Sampler)
			{
				continue;
			}

			float CurrentTime = 0.f;
			FAnimationRuntime::AdvanceTime(false, PlayTime, CurrentTime, PreviewAsset->GetPlayLength());
			const float CurrentScaledTime = PreviewActor.GetScaledTime(CurrentTime);

			const FPoseSearchIndexAsset& IndexAsset = SearchIndex.Assets[PreviewActor.IndexAssetIndex];
			AnimInstance->SetPosition(CurrentScaledTime);
			AnimInstance->SetPlayRate(0.f);
			AnimInstance->SetBlendSpacePosition(IndexAsset.BlendParameters);

			FTransform RootMotion = Sampler->ExtractRootTransform(CurrentTime);
			if (AnimInstance->GetMirrorDataTable())
			{
				RootMotion = MirrorRootMotion(RootMotion, AnimInstance->GetMirrorDataTable());
			}
			PreviewActor.Actor->SetActorTransform(RootMotion);

			PreviewActor.CurrentPoseIndex = PoseSearchDatabase->GetPoseIndexFromTime(CurrentTime, IndexAsset);
		}
	}

	void FDatabaseViewModel::RemovePreviewActors()
	{
		for (auto PreviewActor : PreviewActors)
		{
			// @todo: PreviewActor.Actor is a TWeakObjectPtr so it can be null.
			PreviewActor.Actor->Destroy();
		}
		PreviewActors.Reset();

		MaxPreviewPlayLength = 0.0f;
	}

	FTransform FDatabaseViewModel::MirrorRootMotion(
		FTransform RootMotion,
		const UMirrorDataTable* MirrorDataTable)
	{
		const FTransform RootReferenceTransform = PoseSearchDatabase->Schema->Skeleton->GetReferenceSkeleton().GetRefBonePose()[0];
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
		return EnumHasAnyFlags(PoseFeaturesDrawMode, DrawMode);
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
		return EnumHasAnyFlags(AnimationPreviewMode, PreviewMode);
	}

	void FDatabaseViewModel::OnToggleDisplayRootMotionSpeed()
	{
		DisplayRootMotionSpeed = !DisplayRootMotionSpeed;
	}

	void FDatabaseViewModel::AddSequenceToDatabase(UAnimSequence* AnimSequence)
	{
		FPoseSearchDatabaseSequence NewAsset;
		NewAsset.Sequence = AnimSequence;
		PoseSearchDatabase->AnimationAssets.Add(FInstancedStruct::Make(NewAsset));
	}

	void FDatabaseViewModel::AddBlendSpaceToDatabase(UBlendSpace* BlendSpace)
	{
		FPoseSearchDatabaseBlendSpace NewAsset;
		NewAsset.BlendSpace = BlendSpace;
		PoseSearchDatabase->AnimationAssets.Add(FInstancedStruct::Make(NewAsset));
	}

	void FDatabaseViewModel::AddAnimCompositeToDatabase(UAnimComposite* AnimComposite)
	{
		FPoseSearchDatabaseAnimComposite NewAsset;
		NewAsset.AnimComposite = AnimComposite;
		PoseSearchDatabase->AnimationAssets.Add(FInstancedStruct::Make(NewAsset));
	}

	void FDatabaseViewModel::DeleteFromDatabase(int32 AnimationAssetIndex)
	{
		PoseSearchDatabase->AnimationAssets.RemoveAt(AnimationAssetIndex);
	}

	void FDatabaseViewModel::SetIsEnabled(int32 AnimationAssetIndex, bool bEnabled)
	{
		if (FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = PoseSearchDatabase->GetMutableAnimationAssetBase(AnimationAssetIndex))
		{
			DatabaseAnimationAsset->SetIsEnabled(bEnabled);
		}
	}

	bool FDatabaseViewModel::IsEnabled(int32 AnimationAssetIndex) const
	{
		if (const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = PoseSearchDatabase->GetAnimationAssetBase(AnimationAssetIndex))
		{
			return DatabaseAnimationAsset->IsEnabled();
		}

		return false;
	}

	void FDatabaseViewModel::SetSelectedNode(const TSharedPtr<FDatabaseAssetTreeNode>& InSelectedNode)
	{
		SelectedNodes.Reset(1);
		SelectedNodes.Push(InSelectedNode);
		ResetPreviewActors();
		ProcessSelectedActor(nullptr);
	}

	void FDatabaseViewModel::SetSelectedNodes(const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& InSelectedNodes)
	{
		SelectedNodes = InSelectedNodes;
		ResetPreviewActors();
		ProcessSelectedActor(nullptr);
	}

	void FDatabaseViewModel::ProcessSelectedActor(AActor* Actor)
	{
		SelectedActorIndexAssetIndex = INDEX_NONE;
		if (const FDatabasePreviewActor* SelectedPreviewActor = PreviewActors.FindByPredicate([Actor](const FDatabasePreviewActor& PreviewActor) { return PreviewActor.Actor == Actor; }))
		{
			SelectedActorIndexAssetIndex = SelectedPreviewActor->IndexAssetIndex;
		}
	}

	const FPoseSearchIndexAsset* FDatabaseViewModel::GetSelectedActorIndexAsset() const
	{
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FPoseSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
			if (SearchIndex.Assets.IsValidIndex(SelectedActorIndexAssetIndex))
			{
				return &SearchIndex.Assets[SelectedActorIndexAssetIndex];
			}
		}
		return nullptr;
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
