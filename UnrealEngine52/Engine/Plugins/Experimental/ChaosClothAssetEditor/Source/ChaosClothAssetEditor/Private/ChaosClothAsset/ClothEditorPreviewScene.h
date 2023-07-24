// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "AdvancedPreviewScene.h"
#include "Math/Transform.h"
#include "ClothEditorPreviewScene.generated.h"

class UChaosClothAsset;
class USkeletalMesh;
class UChaosClothComponent;
class FChaosClothPreviewScene;
class UPhysicsAsset;
class UAnimSequence;
class ASkeletalMeshActor;
class FAssetEditorModeManager;

///
/// The UChaosClothPreviewSceneDescription is a description of the Preview scene contents, intended to be editable in an FAdvancedPreviewSettingsWidget
/// 
UCLASS()
class CHAOSCLOTHASSETEDITOR_API UChaosClothPreviewSceneDescription : public UObject
{
public:
	GENERATED_BODY()

	void SetPreviewScene(FChaosClothPreviewScene* PreviewScene);

	// Skeletal Mesh source asset
	UPROPERTY(EditAnywhere, Category="SkeletalMesh")
	TObjectPtr<USkeletalMesh> SkeletalMeshAsset;

	UPROPERTY(EditAnywhere, Category = "SkeletalMesh")
	FTransform SkeletalMeshTransform;

	// TODO: Add anything else to the scene, e.g.:
	//UPROPERTY(EditAnywhere, Category = "Animation")
	//TObjectPtr<UAnimSequence> AnimationSequence;

private:

	// Listen for changes to the scene description members and notify the PreviewScene
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;

	FChaosClothPreviewScene* PreviewScene;
};


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

	void CreateClothActor(UChaosClothAsset* Asset);

	// Update Scene in response to the SceneDescription changing
	void SceneDescriptionPropertyChanged(struct FPropertyChangedEvent& PropertyChangedEvent);

	// Preview simulation mesh
	TObjectPtr<AActor> ClothActor;
	TObjectPtr<UChaosClothComponent> ClothComponent;

	// Skeletal Mesh
	TObjectPtr<ASkeletalMeshActor> SkeletalMeshActor;

	void SetModeManager(TSharedPtr<FAssetEditorModeManager> InClothPreviewEditorModeManager);

private:

	void SkeletalMeshTransformChanged(USceneComponent* UpdatedComponent, EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport);

	void CreateSkeletalMeshActor();

	bool IsComponentSelected(const UPrimitiveComponent* InComponent);

	TObjectPtr<UChaosClothPreviewSceneDescription> PreviewSceneDescription;

	TSharedPtr<FAssetEditorModeManager> ClothPreviewEditorModeManager;
};


