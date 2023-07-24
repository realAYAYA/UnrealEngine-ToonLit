// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "InteractiveToolBuilder.h"
#include "MeshOpPreviewHelpers.h"
#include "Properties/MeshStatisticsProperties.h"
#include "PropertySets/OnAcceptProperties.h"
#include "CompositionOps/VoxelMergeMeshesOp.h"
#include "MergeMeshesTool.generated.h"

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UMergeMeshesToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;

protected:
	virtual const FToolTargetTypeRequirements& GetTargetRequirements() const override;
};



/**
 * Standard properties of the Merge Meshes operation
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UMergeMeshesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:

	/** The size of the geometry bounding box major axis measured in voxels.*/
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "8", UIMax = "1024", ClampMin = "8", ClampMax = "1024"))
	int32 VoxelCount = 128;

	/** Remeshing adaptivity, prior to optional simplification */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float MeshAdaptivity = 0.001f;

	/** Offset when remeshing, note large offsets with high voxels counts will be slow */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "-10", UIMax = "10", ClampMin = "-10", ClampMax = "10"))
	float OffsetDistance = 0;

	/** Automatically simplify the result of voxel-based merge.*/
	UPROPERTY(EditAnywhere, Category = Options)
	bool bAutoSimplify = false;
};

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UMergeMeshesTool : public UMultiSelectionMeshEditingTool, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	UMergeMeshesTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

protected:
	UPROPERTY()
	TObjectPtr<UMergeMeshesToolProperties> MergeProps;

	UPROPERTY()
	TObjectPtr<UMeshStatisticsProperties> MeshStatisticsProperties;

	UPROPERTY()
	TObjectPtr<UOnAcceptHandleSourcesProperties> HandleSourcesProperties;


	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> Preview;

protected:
	TArray<UE::Geometry::FVoxelMergeMeshesOp::FInputMesh> InputMeshes;
	/** stash copies of the transforms and pointers to the meshes for consumption by merge Op*/
	void CacheInputMeshes();

	/** quickly generate a low-quality result for display while the actual result is being computed. */
	void CreateLowQualityPreview();

	void GenerateAsset(const FDynamicMeshOpResult& Result);
};
