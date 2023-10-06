// Copyright Epic Games, Inc. All Rights Reserved.

#include "PoseSearchDatabaseViewModel.h"
#include "AnimPreviewInstance.h"
#include "Animation/AnimComposite.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Animation/MirrorDataTable.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "InstancedStruct.h"
#include "Modules/ModuleManager.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDefines.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearchDatabaseAssetTreeNode.h"
#include "PoseSearchDatabaseDataDetails.h"
#include "PoseSearchDatabasePreviewScene.h"
#include "PoseSearchEditor.h"
#include "PropertyEditorModule.h"

namespace UE::PoseSearch
{
	constexpr float StepDeltaTime = 1.0f / 30.0f;

	// FDatabasePreviewActor
	bool FDatabasePreviewActor::IsValid() const
	{
		return Actor != nullptr;
	}

	void FDatabasePreviewActor::Process(const FBoneContainer& BoneContainer)
	{
		Sampler.Process(BoneContainer);
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

	void FDatabaseViewModel::Initialize(UPoseSearchDatabase* InPoseSearchDatabase, const TSharedRef<FDatabasePreviewScene>& InPreviewScene, const TSharedRef<SDatabaseDataDetails>& InDatabaseDataDetails)
	{
		PoseSearchDatabase = InPoseSearchDatabase;
		PreviewScenePtr = InPreviewScene;
		DatabaseDataDetails = InDatabaseDataDetails;

		RemovePreviewActors();

		PoseSearchDatabase->RegisterOnDerivedDataRebuild(UPoseSearchDatabase::FOnDerivedDataRebuild::CreateSP(this, &FDatabaseViewModel::RemovePreviewActors));
	}

	void FDatabaseViewModel::BuildSearchIndex()
	{
		using namespace UE::PoseSearch;
		FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::NewRequest);
	}

	void FDatabaseViewModel::PreviewBackwardEnd()
	{
		PlayTime = MinPreviewPlayLength;
	}

	void FDatabaseViewModel::PreviewBackwardStep()
	{
		PlayTime = FMath::Clamp(PlayTime - StepDeltaTime, MinPreviewPlayLength, MaxPreviewPlayLength);
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
		PlayTime = FMath::Clamp(PlayTime + StepDeltaTime, MinPreviewPlayLength, MaxPreviewPlayLength);
		DeltaTimeMultiplier = 0.0f;
	}

	void FDatabaseViewModel::PreviewForwardEnd()
	{
		PlayTime = MaxPreviewPlayLength;
	}

	FDatabasePreviewActor FDatabaseViewModel::SpawnPreviewActor(int32 IndexAssetIndex, int32 PoseIdxForTimeOffset)
	{
		FDatabasePreviewActor PreviewActor;

		check(PoseSearchDatabase);
		const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
		const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];

		const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAsset = PoseSearchDatabase->GetAnimationAssetBase(IndexAsset.SourceAssetIdx);
		UAnimationAsset* PreviewAsset = DatabaseAnimationAsset->GetAnimationAsset();
		if (!PreviewAsset)
		{
			return PreviewActor;
		}

		const FInstancedStruct& DatabaseAsset = PoseSearchDatabase->GetAnimationAssetStruct(IndexAsset);
		const FPoseSearchDatabaseAnimationAssetBase* DatabaseAnimationAssetBase = DatabaseAsset.GetPtr<FPoseSearchDatabaseAnimationAssetBase>();
		check(DatabaseAnimationAssetBase);
		PreviewActor.Sampler.Init(DatabaseAnimationAssetBase->GetAnimationAsset(), IndexAsset.BlendParameters);
		PreviewActor.IndexAssetIndex = IndexAssetIndex;
		PreviewActor.CurrentPoseIndex = INDEX_NONE;
		PreviewActor.PlayTimeOffset = PoseIdxForTimeOffset < 0 ? 0.f : PoseSearchDatabase->GetRealAssetTime(PoseIdxForTimeOffset);

		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
		PreviewActor.Actor = GetWorld()->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, Params);
		PreviewActor.Actor->SetFlags(RF_Transient);

		UDebugSkelMeshComponent* Mesh = NewObject<UDebugSkelMeshComponent>(PreviewActor.Actor.Get());
		Mesh->RegisterComponentWithWorld(GetWorld());

		UAnimPreviewInstance* AnimInstance = NewObject<UAnimPreviewInstance>(Mesh);

		Mesh->PreviewInstance = AnimInstance;
		AnimInstance->InitializeAnimation();

		USkeletalMesh* DatabasePreviewMesh = PoseSearchDatabase->PreviewMesh;
		Mesh->SetSkeletalMesh(DatabasePreviewMesh ? DatabasePreviewMesh : PoseSearchDatabase->Schema->Skeleton->GetPreviewMesh(true));
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

	UWorld* FDatabaseViewModel::GetWorld()
	{
		check(PreviewScenePtr.IsValid());
		return PreviewScenePtr.Pin()->GetWorld();
	}

	void FDatabaseViewModel::OnPreviewActorClassChanged()
	{
		// todo: implement
	}

	void FDatabaseViewModel::Tick(float DeltaSeconds)
	{
		const float DeltaPlayTime = DeltaSeconds * DeltaTimeMultiplier;
		if (!FMath::IsNearlyZero(DeltaPlayTime))
		{
			PlayTime += DeltaPlayTime;
			PlayTime = FMath::Clamp(PlayTime, MinPreviewPlayLength, MaxPreviewPlayLength);

			if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
			{
				UpdatePreviewActors(true);
			}
		}
	}

	void FDatabaseViewModel::UpdatePreviewActors(bool bInTickPlayTime)
	{
		const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
		TSharedPtr<FDatabasePreviewScene> PreviewScene = PreviewScenePtr.Pin();
		for (FDatabasePreviewActor& PreviewActor : GetPreviewActors())
		{
			UAnimPreviewInstance* AnimInstance = PreviewActor.GetAnimPreviewInstance();
			if (!AnimInstance || PreviewActor.IndexAssetIndex >= SearchIndex.Assets.Num())
			{
				continue;
			}

			const UAnimationAsset* PreviewAsset = AnimInstance->GetAnimationAsset();
			if (!PreviewAsset || !PreviewActor.Sampler.IsInitialized())
			{
				continue;
			}

			PreviewActor.CurrentTime = 0.f;
			const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[PreviewActor.IndexAssetIndex];
			float CurrentPlayTime = PlayTime + IndexAsset.GetFirstSampleTime(PoseSearchDatabase->Schema->SampleRate) + PreviewActor.PlayTimeOffset;
			FAnimationRuntime::AdvanceTime(false, CurrentPlayTime, PreviewActor.CurrentTime, IndexAsset.GetLastSampleTime(PoseSearchDatabase->Schema->SampleRate));
			 
			// time to pose index
			PreviewActor.CurrentPoseIndex = PoseSearchDatabase->GetPoseIndexFromTime(PreviewActor.CurrentTime, IndexAsset);

			const float QuantizedTime = PreviewActor.CurrentPoseIndex >= 0 ? PoseSearchDatabase->GetRealAssetTime(PreviewActor.CurrentPoseIndex) : PreviewActor.CurrentTime;
			if (bQuantizeAnimationToPoseData)
			{
				PreviewActor.CurrentTime = QuantizedTime;
			}

			// SetPosition is in [0..1] range for blendspaces
			AnimInstance->SetPosition(PreviewActor.Sampler.ToNormalizedTime(PreviewActor.CurrentTime));
			AnimInstance->SetPlayRate(0.f);
			AnimInstance->SetBlendSpacePosition(IndexAsset.BlendParameters);

			PreviewActor.QuantizedTimeRootTransform = PreviewActor.Sampler.ExtractRootTransform(QuantizedTime);
			FTransform RootMotion = PreviewActor.Sampler.ExtractRootTransform(PreviewActor.CurrentTime);
			if (AnimInstance->GetMirrorDataTable())
			{
				RootMotion = MirrorRootTransform(RootMotion);
				PreviewActor.QuantizedTimeRootTransform = MirrorRootTransform(PreviewActor.QuantizedTimeRootTransform);
			}

			if (PreviewActor.PlayTimeOffset != 0.f)
			{
				FTransform OriginRootMotion = PreviewActor.Sampler.ExtractRootTransform(PreviewActor.PlayTimeOffset);
				if (AnimInstance->GetMirrorDataTable())
				{
					OriginRootMotion = MirrorRootTransform(OriginRootMotion);
				}
				RootMotion.SetToRelativeTransform(OriginRootMotion);
				PreviewActor.QuantizedTimeRootTransform.SetToRelativeTransform(OriginRootMotion);
			}

			PreviewActor.Actor->SetActorTransform(RootMotion);
		}
	}

	void FDatabaseViewModel::RemovePreviewActors()
	{
		PlayTime = 0.f;
		DeltaTimeMultiplier = 1.f;
		MaxPreviewPlayLength = 0.f;
		MinPreviewPlayLength = 0.f;
		bIsEditorSelection = true;
		bDrawQueryVector = false;

		for (FDatabasePreviewActor& PreviewActor : PreviewActors)
		{
			PreviewActor.Actor->Destroy();
		}
		PreviewActors.Reset();
	}

	FTransform FDatabaseViewModel::MirrorRootTransform(const FTransform& RootTransform)
	{
		check(PoseSearchDatabase && PoseSearchDatabase->Schema);
		if (PoseSearchDatabase->Schema->MirrorDataTable && PoseSearchDatabase->Schema->Skeleton)
		{
			const FTransform RootReferenceTransform = PoseSearchDatabase->Schema->Skeleton->GetReferenceSkeleton().GetRefBonePose()[0];
			const FQuat RootReferenceRotation = RootReferenceTransform.GetRotation();

			const EAxis::Type MirrorAxis = PoseSearchDatabase->Schema->MirrorDataTable->MirrorAxis;
			FVector T = RootTransform.GetTranslation();
			T = FAnimationRuntime::MirrorVector(T, MirrorAxis);
			FQuat Q = RootTransform.GetRotation();
			Q = FAnimationRuntime::MirrorQuat(Q, MirrorAxis);
			Q *= FAnimationRuntime::MirrorQuat(RootReferenceRotation, MirrorAxis).Inverse() * RootReferenceRotation;

			const FTransform MirroredRootTransform = FTransform(Q, T, RootTransform.GetScale3D());
			return MirroredRootTransform;
		}
		return RootTransform;
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
			RemovePreviewActors();
		}
	}

	bool FDatabaseViewModel::IsAnimationPreviewMode(EAnimationPreviewMode PreviewMode) const
	{
		return EnumHasAnyFlags(AnimationPreviewMode, PreviewMode);
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

	void FDatabaseViewModel::AddAnimMontageToDatabase(UAnimMontage* AnimMontage)
	{
		FPoseSearchDatabaseAnimMontage NewAsset;
		NewAsset.AnimMontage = AnimMontage;
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

	int32 FDatabaseViewModel::SetSelectedNode(int32 PoseIdx, bool bClearSelection, bool bDrawQuery, TConstArrayView<float> InQueryVector)
	{
		int32 SelectedSourceAssetIdx = -1;

		if (bClearSelection)
		{
			RemovePreviewActors();
		}

		bIsEditorSelection = false;
		bDrawQueryVector = bDrawQuery;
		QueryVector = InQueryVector;

		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
			if (SearchIndex.PoseMetadata.IsValidIndex(PoseIdx))
			{
				const uint32 IndexAssetIndex = SearchIndex.PoseMetadata[PoseIdx].GetAssetIndex();
				if (SearchIndex.Assets.IsValidIndex(IndexAssetIndex))
				{
					FDatabasePreviewActor PreviewActor = SpawnPreviewActor(IndexAssetIndex, PoseIdx);
					if (PreviewActor.IsValid())
					{
						const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];
						MaxPreviewPlayLength = FMath::Max(MaxPreviewPlayLength, IndexAsset.GetLastSampleTime(PoseSearchDatabase->Schema->SampleRate) - PreviewActor.PlayTimeOffset);
						MinPreviewPlayLength = FMath::Min(MinPreviewPlayLength, IndexAsset.GetFirstSampleTime(PoseSearchDatabase->Schema->SampleRate) - PreviewActor.PlayTimeOffset);
						PreviewActors.Add(PreviewActor);
						SelectedSourceAssetIdx = IndexAsset.SourceAssetIdx;
					}
				}
			}

			FBoneContainer BoneContainer;
			BoneContainer.InitializeTo(PoseSearchDatabase->Schema->BoneIndicesWithParents, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *PoseSearchDatabase->Schema->Skeleton);
			// capturing BoneContainer by copy, since it has mutable properties
			ParallelFor(PreviewActors.Num(), [this, BoneContainer](int32 PreviewActorIndex) { PreviewActors[PreviewActorIndex].Process(BoneContainer); }, ParallelForFlags);

			DatabaseDataDetails.Pin()->Reconstruct();

			UpdatePreviewActors();

			SetPlayTime(0.f, false);
		}

		ProcessSelectedActor(nullptr);

		return SelectedSourceAssetIdx;
	}

	void FDatabaseViewModel::SetSelectedNodes(const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& InSelectedNodes)
	{
		RemovePreviewActors();

		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
		{
			TMap<int32, int32> AssociatedAssetIndices;
			for (int32 i = 0; i < InSelectedNodes.Num(); ++i)
			{
				AssociatedAssetIndices.FindOrAdd(InSelectedNodes[i]->SourceAssetIdx) = i;
			}

			const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
			for (int32 IndexAssetIndex = 0; IndexAssetIndex < SearchIndex.Assets.Num(); ++IndexAssetIndex)
			{
				const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[IndexAssetIndex];
				if (AnimationPreviewMode == EAnimationPreviewMode::OriginalAndMirrored || !IndexAsset.bMirrored)
				{
					if (const int32* SelectedNodesIndex = AssociatedAssetIndices.Find(IndexAsset.SourceAssetIdx))
					{
						FDatabasePreviewActor PreviewActor = SpawnPreviewActor(IndexAssetIndex);
						if (PreviewActor.IsValid())
						{
							MaxPreviewPlayLength = FMath::Max(MaxPreviewPlayLength, IndexAsset.GetLastSampleTime(PoseSearchDatabase->Schema->SampleRate) - IndexAsset.GetFirstSampleTime(PoseSearchDatabase->Schema->SampleRate));
							PreviewActors.Add(PreviewActor);
						}
					}
				}
			}

			FBoneContainer BoneContainer;
			BoneContainer.InitializeTo(PoseSearchDatabase->Schema->BoneIndicesWithParents, UE::Anim::FCurveFilterSettings(UE::Anim::ECurveFilterMode::DisallowAll), *PoseSearchDatabase->Schema->Skeleton);
			// capturing BoneContainer by copy, since it has mutable properties
			ParallelFor(PreviewActors.Num(), [this, BoneContainer](int32 PreviewActorIndex) { PreviewActors[PreviewActorIndex].Process(BoneContainer); }, ParallelForFlags);

			DatabaseDataDetails.Pin()->Reconstruct();
			UpdatePreviewActors();
		}

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

	void FDatabaseViewModel::SetDrawQueryVector(bool bValue)
	{
		if (bDrawQueryVector != bValue)
		{
			bDrawQueryVector = bValue;
			DatabaseDataDetails.Pin()->Reconstruct();
		}
	}

	const FSearchIndexAsset* FDatabaseViewModel::GetSelectedActorIndexAsset() const
	{
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
			if (SearchIndex.Assets.IsValidIndex(SelectedActorIndexAssetIndex))
			{
				return &SearchIndex.Assets[SelectedActorIndexAssetIndex];
			}
		}
		return nullptr;
	}

	TRange<double> FDatabaseViewModel::GetPreviewPlayRange() const
	{
		constexpr double ViewRangeSlack = 0.2;
		return TRange<double>(MinPreviewPlayLength - ViewRangeSlack, MaxPreviewPlayLength + ViewRangeSlack);
	}

	float FDatabaseViewModel::GetPlayTime() const
	{
		return PlayTime;
	}

	void FDatabaseViewModel::SetPlayTime(float NewPlayTime, bool bInTickPlayTime)
	{
		NewPlayTime = FMath::Clamp(NewPlayTime, MinPreviewPlayLength, MaxPreviewPlayLength);
		DeltaTimeMultiplier = bInTickPlayTime ? DeltaTimeMultiplier : 0.0f;

		if (!FMath::IsNearlyEqual(PlayTime, NewPlayTime))
		{
			PlayTime = NewPlayTime;
			if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
			{
				UpdatePreviewActors(true);
			}
		}
	}

	bool FDatabaseViewModel::GetAnimationTime(int32 SourceAssetIdx, float& CurrentPlayTime, FVector& BlendParameters) const
	{
		if (FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(PoseSearchDatabase, ERequestAsyncBuildFlag::ContinueRequest))
		{
			const FSearchIndex& SearchIndex = PoseSearchDatabase->GetSearchIndex();
			for (const FDatabasePreviewActor& PreviewActor : GetPreviewActors())
			{
				if (PreviewActor.IndexAssetIndex >= 0 && PreviewActor.IndexAssetIndex < SearchIndex.Assets.Num() && PreviewActor.Sampler.IsInitialized())
				{
					const FSearchIndexAsset& IndexAsset = SearchIndex.Assets[PreviewActor.IndexAssetIndex];
					if (IndexAsset.SourceAssetIdx == SourceAssetIdx)
					{
						CurrentPlayTime = PreviewActor.Sampler.ToNormalizedTime(PlayTime + IndexAsset.GetFirstSampleTime(PoseSearchDatabase->Schema->SampleRate) + PreviewActor.PlayTimeOffset);
						BlendParameters = IndexAsset.BlendParameters;
						return true;
					}
				}
			}

			for (const FSearchIndexAsset& IndexAsset : SearchIndex.Assets)
			{
				if (IndexAsset.SourceAssetIdx == SourceAssetIdx)
				{
					CurrentPlayTime = PlayTime + IndexAsset.GetFirstSampleTime(PoseSearchDatabase->Schema->SampleRate);
					BlendParameters = IndexAsset.BlendParameters;

					const bool bIsBlendSpace = PoseSearchDatabase->GetAnimationAssetStruct(IndexAsset).GetPtr<FPoseSearchDatabaseBlendSpace>() != nullptr;
					if (bIsBlendSpace && !FMath::IsNearlyEqual(MaxPreviewPlayLength, MinPreviewPlayLength))
					{
						CurrentPlayTime = (CurrentPlayTime - MaxPreviewPlayLength) / (MaxPreviewPlayLength - MinPreviewPlayLength);
					}
					return true;
				}
			}
		}

		CurrentPlayTime = 0.f;
		BlendParameters = FVector::ZeroVector;
		return false;
	}
}
