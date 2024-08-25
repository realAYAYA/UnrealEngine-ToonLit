// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/SingleSelectionMeshEditingTool.h"
#include "ModelingOperators.h"
#include "Transforms/TransformGizmoDataBinder.h"
#include "ClothTransferSkinWeightsTool.generated.h"


class UClothTransferSkinWeightsTool;
class USkeletalMesh;
class UClothEditorContextObject;
class UTransformProxy;
class UCombinedTransformGizmo;
class UMeshOpPreviewWithBackgroundCompute;
class AInternalToolFrameworkActor;
class USkeletalMeshComponent;
class FTransformGizmoDataBinder;
struct FChaosClothAssetTransferSkinWeightsNode;

UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTransferSkinWeightsToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Category = Source)
	TObjectPtr<USkeletalMesh> SourceMesh;

	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DisplayName = "Location", EditCondition = "SourceMesh != nullptr"))
	FVector3d SourceMeshTranslation = FVector3d::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DisplayName = "Rotation", EditCondition = "SourceMesh != nullptr"))
	FVector3d SourceMeshRotation = FVector3d::ZeroVector;

	UPROPERTY(EditAnywhere, Category = "Transform", meta = (DisplayName = "Scale", AllowPreserveRatio, EditCondition = "SourceMesh != nullptr"))
	FVector3d SourceMeshScale = FVector3d::OneVector;

	UPROPERTY(EditAnywhere, Category = Source)
	bool bHideSourceMesh = false;
};

UCLASS()
class CHAOSCLOTHASSETEDITORTOOLS_API UClothTransferSkinWeightsTool : public USingleSelectionMeshEditingTool, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

private:

	friend class UClothTransferSkinWeightsToolBuilder;

	// UInteractiveTool
	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;
	virtual bool HasAccept() const override { return true; }
	virtual bool HasCancel() const override { return true; }
	virtual bool CanAccept() const override;
	virtual void OnTick(float DeltaTime) override;

	// IDynamicMeshOperatorFactory
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;
	
	void SetClothEditorContextObject(TObjectPtr<UClothEditorContextObject> InClothEditorContextObject);

	FTransform TransformFromProperties() const;
	void SetSRTPropertiesFromTransform(const FTransform& Transform) const;

	void UpdateSourceMesh(TObjectPtr<USkeletalMesh> Mesh);

	void OpFinishedCallback(const UE::Geometry::FDynamicMeshOperator* Op);

	void PreviewMeshUpdatedCallback(UMeshOpPreviewWithBackgroundCompute* Preview);


	UPROPERTY(Transient)
	TObjectPtr<UClothTransferSkinWeightsToolProperties> ToolProperties;

	UPROPERTY(Transient)
	TObjectPtr<UClothEditorContextObject> ClothEditorContextObject;

	UPROPERTY(Transient)
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> TargetClothPreview;

	UPROPERTY(Transient)
	TObjectPtr<AInternalToolFrameworkActor> SourceMeshParentActor;

	UPROPERTY(Transient)
	TObjectPtr<USkeletalMeshComponent> SourceMeshComponent;

	// Source mesh transform gizmo support
	UPROPERTY(Transient)
	TObjectPtr<UTransformProxy> SourceMeshTransformProxy;

	UPROPERTY(Transient)
	TObjectPtr<UCombinedTransformGizmo> SourceMeshTransformGizmo;

	TSharedPtr<FTransformGizmoDataBinder> DataBinder;

	FChaosClothAssetTransferSkinWeightsNode* TransferSkinWeightsNode = nullptr;

	bool bHasOpFailedWarning = false;
};


