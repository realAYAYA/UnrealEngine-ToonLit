// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InstancedStruct.h"
#include "MovieSceneFwd.h"
#include "PoseSearch/PoseSearchAssetSampler.h"
#include "PoseSearch/PoseSearchIndex.h"
#include "PoseSearchDatabasePreviewScene.h"
#include "UObject/GCObject.h"

class UWorld;
class UPoseSearchDatabase;
class UAnimPreviewInstance;
class UDebugSkelMeshComponent;
class UAnimComposite;
class UAnimSequence;
class UBlendSpace;
class UMirrorDataTable;

namespace UE::PoseSearch
{
	class FDatabaseAssetTreeNode;
	struct FSearchIndexAsset;
	class SDatabaseDataDetails;

	enum class EFeaturesDrawMode : uint8
	{
		None,
		All,
		Detailed
	};
	ENUM_CLASS_FLAGS(EFeaturesDrawMode);

	enum class EAnimationPreviewMode : uint8
	{
		OriginalOnly,
		OriginalAndMirrored
	};
	ENUM_CLASS_FLAGS(EAnimationPreviewMode);

	struct FDatabasePreviewActor
	{
	public:
		TObjectPtr<AActor> Actor;
		int32 IndexAssetIndex = INDEX_NONE;
		int32 CurrentPoseIndex = INDEX_NONE;
		float PlayTimeOffset = 0.f;
		float CurrentTime = 0.f;
		FAnimationAssetSampler Sampler;
		FTransform QuantizedTimeRootTransform = FTransform::Identity;

		bool IsValid() const;
		void Process(const FBoneContainer& BoneContainer);
		UDebugSkelMeshComponent* GetDebugSkelMeshComponent();
		UAnimPreviewInstance* GetAnimPreviewInstance();
	};

	class FDatabaseViewModel : public TSharedFromThis<FDatabaseViewModel>, public FGCObject
	{
	public:

		FDatabaseViewModel();
		virtual ~FDatabaseViewModel();

		// ~ FGCObject interface
		virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
		virtual FString GetReferencerName() const override { return TEXT("FPoseSearchDatabaseViewModel"); }

		void Initialize(UPoseSearchDatabase* InPoseSearchDatabase, const TSharedRef<FDatabasePreviewScene>& InPreviewScene, const TSharedRef<SDatabaseDataDetails>& InDatabaseDataDetails);

		void RemovePreviewActors();
		void BuildSearchIndex();

		void PreviewBackwardEnd();
		void PreviewBackwardStep();
		void PreviewBackward();
		void PreviewPause();
		void PreviewForward();
		void PreviewForwardStep();
		void PreviewForwardEnd();

		UPoseSearchDatabase* GetPoseSearchDatabase() { return PoseSearchDatabase; }
		void OnPreviewActorClassChanged();

		void Tick(float DeltaSeconds);

		TArray<FDatabasePreviewActor>& GetPreviewActors() { return PreviewActors; }
		const TArray<FDatabasePreviewActor>& GetPreviewActors() const { return PreviewActors; }

		void OnSetPoseFeaturesDrawMode(EFeaturesDrawMode DrawMode);
		bool IsPoseFeaturesDrawMode(EFeaturesDrawMode DrawMode) const;

		void OnSetAnimationPreviewMode(EAnimationPreviewMode PreviewMode);
		bool IsAnimationPreviewMode(EAnimationPreviewMode PreviewMode) const;

		void OnToggleDisplayRootMotionSpeed() { bDisplayRootMotionSpeed = !bDisplayRootMotionSpeed; }
		bool IsDisplayRootMotionSpeedChecked() const { return bDisplayRootMotionSpeed; };

		void OnToggleQuantizeAnimationToPoseData() { bQuantizeAnimationToPoseData = !bQuantizeAnimationToPoseData; }
		bool IsQuantizeAnimationToPoseDataChecked() const { return bQuantizeAnimationToPoseData; };

		void OnToggleShowBones() { bShowBones = !bShowBones; }
		bool IsShowBones() const { return bShowBones; };

		void AddSequenceToDatabase(UAnimSequence* AnimSequence);
		void AddBlendSpaceToDatabase(UBlendSpace* BlendSpace);
		void AddAnimCompositeToDatabase(UAnimComposite* AnimComposite);
		void AddAnimMontageToDatabase(UAnimMontage* AnimMontage);
		void DeleteFromDatabase(int32 AnimationAssetIndex);

		void SetIsEnabled(int32 AnimationAssetIndex, bool bEnabled);
		bool IsEnabled(int32 AnimationAssetIndex) const;

		int32 SetSelectedNode(int32 PoseIdx, bool bClearSelection, bool bDrawQuery, TConstArrayView<float> InQueryVector);
		void SetSelectedNodes(const TArrayView<TSharedPtr<FDatabaseAssetTreeNode>>& InSelectedNodes);
		void ProcessSelectedActor(AActor* Actor);
		
		TConstArrayView<float> GetQueryVector() const { return QueryVector; }
		void SetDrawQueryVector(bool bValue);
		bool ShouldDrawQueryVector() const { return bDrawQueryVector && !bIsEditorSelection; }

		const FSearchIndexAsset* GetSelectedActorIndexAsset() const;

		TRange<double> GetPreviewPlayRange() const;

		void SetPlayTime(float NewPlayTime, bool bInTickPlayTime);
		float GetPlayTime() const;
		bool IsEditorSelection() const { return bIsEditorSelection; }
		bool GetAnimationTime(int32 SourceAssetIdx, float& CurrentPlayTime, FVector& BlendParameters) const;


	private:
		float PlayTime = 0.f;
		float DeltaTimeMultiplier = 1.f;

		/** Scene asset being viewed and edited by this view model. */
		TObjectPtr<UPoseSearchDatabase> PoseSearchDatabase;

		/** Weak pointer to the PreviewScene */
		TWeakPtr<FDatabasePreviewScene> PreviewScenePtr;

		/** Weak pointer to the SDatabaseDataDetails */
		TWeakPtr<SDatabaseDataDetails> DatabaseDataDetails;

		/** Actors to be displayed in the preview viewport */
		TArray<FDatabasePreviewActor> PreviewActors;
		
		/** From zero to the play length of the longest preview */
		float MaxPreviewPlayLength = 0.f;
		float MinPreviewPlayLength = 0.f;

		bool bIsEditorSelection = true;
		bool bDrawQueryVector = false;
		TArray<float> QueryVector;

		/** What features to show in the viewport */
		EFeaturesDrawMode PoseFeaturesDrawMode = EFeaturesDrawMode::All;

		/** What animations to show in the viewport */
		EAnimationPreviewMode AnimationPreviewMode = EAnimationPreviewMode::OriginalAndMirrored;

		/** Is animation debug draw enabled */
		bool bDisplayRootMotionSpeed = false;

		bool bQuantizeAnimationToPoseData = false;

		bool bShowBones = false;

		int32 SelectedActorIndexAssetIndex = INDEX_NONE;

		UWorld* GetWorld();

		FDatabasePreviewActor SpawnPreviewActor(int32 IndexAssetIndex, int32 PoseIdxForTimeOffset = -1);

		void UpdatePreviewActors(bool bInTickPlayTime = false);

		FTransform MirrorRootTransform(const FTransform& RootTransform);
	};
}
