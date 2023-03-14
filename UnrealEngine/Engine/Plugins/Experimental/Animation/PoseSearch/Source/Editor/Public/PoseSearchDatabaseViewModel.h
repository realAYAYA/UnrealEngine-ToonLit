// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/GCObject.h"
#include "MovieSceneFwd.h"

#include "PoseSearchDatabasePreviewScene.h"
#include "PoseSearch/PoseSearch.h"

class UWorld;
class UPoseSearchDatabase;
struct FPoseSearchIndexAsset;
class UAnimPreviewInstance;
class UDebugSkelMeshComponent;
class UAnimSequence;
class UBlendSpace;
class UMirrorDataTable;

namespace UE::PoseSearch
{
	class FDatabaseAssetTreeNode;

	enum class EFeaturesDrawMode : uint8
	{
		None,
		All
	};

	enum class EAnimationPreviewMode : uint8
	{
		None,
		OriginalOnly,
		OriginalAndMirrored
	};


	struct FDatabasePreviewActor
	{
	public:
		TWeakObjectPtr<AActor> Actor = nullptr;
		TWeakObjectPtr<UDebugSkelMeshComponent> Mesh = nullptr;
		TWeakObjectPtr<UAnimPreviewInstance> AnimInstance = nullptr;
		FPoseSearchIndexAsset IndexAsset; // keeping a copy since database index can be invalidated
		int32 CurrentPoseIndex = INDEX_NONE;

		bool IsValid()
		{
			const bool bIsValid = Actor.IsValid() && Mesh.IsValid() && AnimInstance.IsValid();
			return  bIsValid;
		}
	};

	class FDatabaseViewModel : public TSharedFromThis<FDatabaseViewModel>, public FGCObject
	{
	public:

		FDatabaseViewModel();
		virtual ~FDatabaseViewModel();

		// ~ FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FPoseSearchDatabaseViewModel"); }

		void Initialize(
			UPoseSearchDatabase* InPoseSearchDatabase,
			const TSharedRef<FDatabasePreviewScene>& InPreviewScene);

		void RemovePreviewActors();
		void ResetPreviewActors();
		void RespawnPreviewActors();
		void BuildSearchIndex();

		void PreviewBackwardEnd();
		void PreviewBackwardStep();
		void PreviewBackward();
		void PreviewPause();
		void PreviewForward();
		void PreviewForwardStep();
		void PreviewForwardEnd();

		UPoseSearchDatabase* GetPoseSearchDatabase() const { return PoseSearchDatabase; }
		void OnPreviewActorClassChanged();

		void Tick(float DeltaSeconds);

		TArray<FDatabasePreviewActor>& GetPreviewActors() { return PreviewActors; }
		const TArray<FDatabasePreviewActor>& GetPreviewActors() const { return PreviewActors; }

		void OnSetPoseFeaturesDrawMode(EFeaturesDrawMode DrawMode);
		bool IsPoseFeaturesDrawMode(EFeaturesDrawMode DrawMode) const;

		void OnSetAnimationPreviewMode(EAnimationPreviewMode PreviewMode);
		bool IsAnimationPreviewMode(EAnimationPreviewMode PreviewMode) const;

		void AddSequenceToDatabase(UAnimSequence* AnimSequence);
		void AddBlendSpaceToDatabase(UBlendSpace* BlendSpace);

		void DeleteSequenceFromDatabase(int32 SequenceIdx);
		void DeleteBlendSpaceFromDatabase(int32 BlendSpaceIdx);

		void SetSelectedSequenceEnabled(int32 SequenceIndex, bool bEnabled);
		void SetSelectedBlendSpaceEnabled(int32 BlendSpaceIndex, bool bEnabled);
		
		bool IsSelectedSequenceEnabled(int32 SequenceIndex) const;
		bool IsSelectedBlendSpaceEnabled(int32 BlendSpaceIndex) const;

		void SetSelectedNodes(const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& InSelectedNodes);
		void ProcessSelectedActor(AActor* Actor);
		
		const FPoseSearchIndexAsset* GetSelectedActorIndexAsset() const { return SelectedActorIndexAsset; }

		float GetMaxPreviewPlayLength() const;
		float GetPlayTime() const;
		void SetPlayTime(float NewPlayTime, bool bInTickPlayTime);

	private:
		float PlayTime = 0.0f;
		float DeltaTimeMultiplier = 1.0f;

		/** Scene asset being viewed and edited by this view model. */
		TObjectPtr<UPoseSearchDatabase> PoseSearchDatabase;

		/** Weak pointer to the PreviewScene */
		TWeakPtr<FDatabasePreviewScene> PreviewScenePtr;

		/** Actors to be displayed in the preview viewport */
		TArray<FDatabasePreviewActor> PreviewActors;

		/** From zero to the play length of the longest preview */
		float MaxPreviewPlayLength = 0.0f;

		/** What features to show in the viewport */
		EFeaturesDrawMode PoseFeaturesDrawMode = EFeaturesDrawMode::All;

		/** What animations to show in the viewport */
		EAnimationPreviewMode AnimationPreviewMode = EAnimationPreviewMode::OriginalAndMirrored;

		TArray<TSharedPtr<FDatabaseAssetTreeNode>> SelectedNodes;

		const FPoseSearchIndexAsset* SelectedActorIndexAsset = nullptr;

		UWorld* GetWorld() const;

		UObject* GetPlaybackContext() const;

		FDatabasePreviewActor SpawnPreviewActor(const FPoseSearchIndexAsset& IndexAsset);

		void UpdatePreviewActors();

		FTransform MirrorRootMotion(FTransform RootMotion, const class UMirrorDataTable* MirrorDataTable);
	};
}
