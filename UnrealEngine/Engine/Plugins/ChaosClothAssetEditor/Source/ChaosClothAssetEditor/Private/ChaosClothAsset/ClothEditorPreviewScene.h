// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedPreviewScene.h"
#include "Math/Transform.h"
#include "ClothEditorPreviewScene.generated.h"

class UChaosClothAsset;
class USkeletalMesh;
class UChaosClothComponent;
class FAssetEditorModeManager;
class UAnimationAsset;
class UAnimSingleNodeInstance;
class FTransformGizmoDataBinder;

namespace UE::Chaos::ClothAsset
{
class FChaosClothPreviewScene;
}

DECLARE_EVENT(UChaosClothPreviewSceneDescription, FClothPreviewSceneDescriptionChanged)

///
/// The UChaosClothPreviewSceneDescription is a description of the Preview scene contents, intended to be editable in an FAdvancedPreviewSettingsWidget
/// 
UCLASS()
class CHAOSCLOTHASSETEDITOR_API UChaosClothPreviewSceneDescription : public UObject
{
public:
	GENERATED_BODY()

	FClothPreviewSceneDescriptionChanged ClothPreviewSceneDescriptionChanged;

	UChaosClothPreviewSceneDescription()
	{
		SetFlags(RF_Transactional);
	}

	void SetPreviewScene(UE::Chaos::ClothAsset::FChaosClothPreviewScene* PreviewScene);

	// Skeletal Mesh source asset
	UPROPERTY(EditAnywhere, Transient, Category="SkeletalMesh")
	TObjectPtr<USkeletalMesh> SkeletalMeshAsset;

	UPROPERTY(EditAnywhere, Transient, Category = "SkeletalMesh")
	TObjectPtr<UAnimationAsset> AnimationAsset;

	UPROPERTY(EditAnywhere, Transient, Category = "SkeletalMesh")
	bool bPostProcessBlueprint;

	UPROPERTY(EditAnywhere, Transient, Category = "Transform", Meta=(DisplayName="Location"))
	FVector3d Translation = FVector3d::ZeroVector;

	UPROPERTY(EditAnywhere, Transient, Category = "Transform")
	FVector3d Rotation = FVector3d::ZeroVector;

	UPROPERTY(EditAnywhere, Transient, Category = "Transform", Meta = (AllowPreserveRatio))
	FVector3d Scale = FVector3d::OneVector;

	// TODO: We should be able to hook this boolean property up to the EditCondition meta tag for the properties above and toggle it
	// on and off when the selection changes in the scene. However the EditCondition does not seem to propagate for some reason, 
	// even if we manually call PostEditChangeProperty() after toggling it. It will take some more digging to figure out exactly
	// what's going on. (UE-189504)
	//UPROPERTY(Transient)
	//bool bValidSelectionForTransform = true;

private:

	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;

	UE::Chaos::ClothAsset::FChaosClothPreviewScene* PreviewScene;
};


namespace UE::Chaos::ClothAsset
{
///
/// FChaosClothPreviewScene is the actual Preview scene, with contents specified by the SceneDescription
/// 
class CHAOSCLOTHASSETEDITOR_API FChaosClothPreviewScene : public FAdvancedPreviewScene
{
public:

	FChaosClothPreviewScene(FPreviewScene::ConstructionValues ConstructionValues);
	virtual ~FChaosClothPreviewScene();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	const UChaosClothPreviewSceneDescription* GetPreviewSceneDescription() const { return PreviewSceneDescription; }
	UChaosClothPreviewSceneDescription* GetPreviewSceneDescription() { return PreviewSceneDescription; }

	void SetClothAsset(UChaosClothAsset* Asset);

	// Update Scene in response to the SceneDescription changing
	void SceneDescriptionPropertyChanged(const FName& PropertyName);

	UAnimSingleNodeInstance* GetPreviewAnimInstance();
	const UAnimSingleNodeInstance* const GetPreviewAnimInstance() const;

	UChaosClothComponent* GetClothComponent();
	const UChaosClothComponent* GetClothComponent() const;
	
	const USkeletalMeshComponent* GetSkeletalMeshComponent() const;

	void SetModeManager(TSharedPtr<FAssetEditorModeManager> InClothPreviewEditorModeManager);
	const TSharedPtr<const FAssetEditorModeManager> GetClothPreviewEditorModeManager() const;

	void SetGizmoDataBinder(TSharedPtr<FTransformGizmoDataBinder> InDataBinder);

private:

	// Create the PreviewAnimationInstance if the AnimationAsset and SkeletalMesh both exist, and set the animation to run on the SkeletalMeshComponent
	void UpdateSkeletalMeshAnimation();

	// Attach the cloth component to the skeletal mesh component, if it exists
	void UpdateClothComponentAttachment();

	bool IsComponentSelected(const UPrimitiveComponent* InComponent);

	TObjectPtr<UChaosClothPreviewSceneDescription> PreviewSceneDescription;

	TSharedPtr<FAssetEditorModeManager> ClothPreviewEditorModeManager;

	TObjectPtr<UAnimSingleNodeInstance> PreviewAnimInstance;

	TObjectPtr<AActor> SceneActor;

	TObjectPtr<UChaosClothComponent> ClothComponent;

	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	TSharedPtr<FTransformGizmoDataBinder> DataBinder = nullptr;
};
} // namespace UE::Chaos::ClothAsset

