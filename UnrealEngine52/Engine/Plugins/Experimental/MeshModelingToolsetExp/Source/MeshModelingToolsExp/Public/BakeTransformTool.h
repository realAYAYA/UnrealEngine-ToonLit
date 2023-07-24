// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "MeshOpPreviewHelpers.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "BaseTools/SingleClickTool.h"
#include "BakeTransformTool.generated.h"


// predeclarations
struct FMeshDescription;
class UDynamicMeshComponent;


/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UBakeTransformToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};


UENUM()
enum class EBakeScaleMethod : uint8
{
	// bake all scale information, so the component has scale of 1 on all axes
	BakeFullScale,
	// bake the non-uniform scale, so the component has a uniform scale
	BakeNonuniformScale,
	// do not bake any scaling
	DoNotBakeScale
};


/**
 * Standard properties
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UBakeTransformToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:
	UBakeTransformToolProperties();

	/** Bake rotation */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bBakeRotation = true;

	/** Bake scale */
	UPROPERTY(EditAnywhere, Category = Options)
	EBakeScaleMethod BakeScale = EBakeScaleMethod::BakeFullScale;

	/** Recenter pivot after baking transform */
	UPROPERTY(EditAnywhere, Category = Options)
	bool bRecenterPivot = false;

	// This variable is used to enable/disable the "DoNotBakeScale" option in the BakeScale enum
	// It is marked as visible only so that the FBakeTransformToolDetails can read it; it will be hidden by that DetailCustomization
	UPROPERTY(VisibleAnywhere, Category = Options)
	bool bAllowNoScale = true;
};



/**
 * Simple tool to bake scene transform on meshes into the mesh assets
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UBakeTransformTool : public UMultiSelectionMeshEditingTool
{
	GENERATED_BODY()

public:
	UBakeTransformTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }

protected:

	UPROPERTY()
	TObjectPtr<UBakeTransformToolProperties> BasicProperties;

protected:
	TArray<int> MapToFirstOccurrences;

	void UpdateAssets();
};
