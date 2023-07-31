// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/BaseCreateFromSelectedTool.h"
#include "PropertySets/VoxelProperties.h"

#include "BaseVoxelTool.generated.h"

PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);

/**
 * Base for Voxel tools
 */
UCLASS()
class MODELINGCOMPONENTS_API UBaseVoxelTool : public UBaseCreateFromSelectedTool
{
	GENERATED_BODY()

protected:

	/** Sets up VoxProperties; typically need to overload and call "Super::SetupProperties();" */
	virtual void SetupProperties() override;

	/** Saves VoxProperties; typically need to overload and call "Super::SaveProperties();" */
	virtual void SaveProperties() override;

	/** Sets up the default preview and converted inputs for voxel tools; Typically do not need to overload */
	virtual void ConvertInputsAndSetPreviewMaterials(bool bSetPreviewMesh = true) override;

	/** Sets the output material to the default "world grid" material */
	virtual TArray<UMaterialInterface*> GetOutputMaterials() const override;


	UPROPERTY()
	TObjectPtr<UVoxelProperties> VoxProperties;

	TArray<TSharedPtr<FDynamicMesh3, ESPMode::ThreadSafe>> OriginalDynamicMeshes;
};

