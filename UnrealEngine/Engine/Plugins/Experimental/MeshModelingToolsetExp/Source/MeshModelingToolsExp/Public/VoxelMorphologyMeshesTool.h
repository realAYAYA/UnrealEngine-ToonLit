// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BaseTools/BaseVoxelTool.h"
#include "CompositionOps/VoxelMorphologyMeshesOp.h"

#include "VoxelMorphologyMeshesTool.generated.h"



/**
 * Properties of the morphology tool
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UVoxelMorphologyMeshesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()

public:

	UPROPERTY(EditAnywhere, Category = Morphology)
	EMorphologyOperation Operation = EMorphologyOperation::Dilate;

	UPROPERTY(EditAnywhere, Category = Morphology, meta = (UIMin = ".1", UIMax = "100", ClampMin = ".001", ClampMax = "1000"))
	double Distance = 5;

	/** Apply a "VoxWrap" operation to input mesh(es) before computing the morphology.  Fixes results for inputs with holes and/or self-intersections. */
	UPROPERTY(EditAnywhere, Category = VoxWrapPreprocess)
	bool bVoxWrap = false;

	/** Remove internal surfaces from the VoxWrap output, before computing the morphology. */
	UPROPERTY(EditAnywhere, Category = VoxWrapPreprocess, meta = (EditCondition = "bVoxWrap == true"))
	bool bRemoveInternalsAfterVoxWrap = false;

	/** Thicken open-boundary surfaces (extrude them inwards) before VoxWrapping them. Units are in world space. If 0, no extrusion is applied. */
	UPROPERTY(EditAnywhere, Category = VoxWrapPreprocess, meta = (EditCondition = "bVoxWrap == true", UIMin = "0", UIMax = "100", ClampMin = "0", ClampMax = "1000"))
	double ThickenShells = 0.0;
};



/**
 * Morphology tool -- dilate, contract, close, open operations on the input shape
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UVoxelMorphologyMeshesTool : public UBaseVoxelTool
{
	GENERATED_BODY()

public:

	UVoxelMorphologyMeshesTool() {}

protected:

	virtual void SetupProperties() override;
	virtual void SaveProperties() override;

	virtual FString GetCreatedAssetName() const override;
	virtual FText GetActionName() const override;

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<UVoxelMorphologyMeshesToolProperties> MorphologyProperties;

};




UCLASS()
class MESHMODELINGTOOLSEXP_API UVoxelMorphologyMeshesToolBuilder : public UBaseCreateFromSelectedToolBuilder
{
	GENERATED_BODY()

public:
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override
	{
		return NewObject<UVoxelMorphologyMeshesTool>(SceneState.ToolManager);
	}
};

