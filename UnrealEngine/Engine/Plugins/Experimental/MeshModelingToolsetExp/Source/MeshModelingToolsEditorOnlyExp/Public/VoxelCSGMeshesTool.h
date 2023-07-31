// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractiveToolBuilder.h"
#include "MeshOpPreviewHelpers.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "Properties/MeshStatisticsProperties.h"
#include "PropertySets/OnAcceptProperties.h"
#include "CompositionOps/VoxelBooleanMeshesOp.h"

#include "VoxelCSGMeshesTool.generated.h"

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UVoxelCSGMeshesToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()

public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};


/**  */
UENUM()
enum class EVoxelCSGOperation : uint8
{
	/** Subtracts the first object from the second */
	DifferenceAB = 0 UMETA(DisplayName = "A - B"),

	/** Subtracts the second object from the first */
	DifferenceBA = 1 UMETA(DisplayName = "B - A"),

	/** intersection of two objects */
	Intersect = 2 UMETA(DisplayName = "Intersect"),

	/** union of two objects */
	Union = 3 UMETA(DisplayName = "Union"),

};


/**
 * Standard properties of the Voxel CSG operation
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UVoxelCSGMeshesToolProperties : public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	/** The type of operation  */
	UPROPERTY(EditAnywhere, Category = Options)
	EVoxelCSGOperation Operation = EVoxelCSGOperation::DifferenceAB;

	/** The size of the geometry bounding box major axis measured in voxels.*/
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "8", UIMax = "1024", ClampMin = "8", ClampMax = "1024"))
	int32 VoxelCount = 128;

	/** Remeshing adaptivity, prior to optional simplification */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "0", UIMax = "1", ClampMin = "0", ClampMax = "1"))
	float MeshAdaptivity = 0.01f;

	/** Offset when remeshing, note large offsets with high voxels counts will be slow */
	UPROPERTY(EditAnywhere, Category = Options, meta = (UIMin = "-10", UIMax = "10", ClampMin = "-10", ClampMax = "10"))
	float OffsetDistance = 0.0f;

	/** Automatically simplify the result of voxel-based merge.*/
	UPROPERTY(EditAnywhere, Category = Options)
	bool bAutoSimplify = false;
};




/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API UVoxelCSGMeshesTool : public UMultiSelectionMeshEditingTool, public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	UVoxelCSGMeshesTool();

	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	virtual void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;


protected:
	UPROPERTY()
	TObjectPtr<UVoxelCSGMeshesToolProperties> CSGProps;

	UPROPERTY()
	TObjectPtr<UMeshStatisticsProperties> MeshStatisticsProperties;

	UPROPERTY()
	TObjectPtr<UOnAcceptHandleSourcesProperties> HandleSourcesProperties;

	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> Preview;

protected:
	TArray<UE::Geometry::FVoxelBooleanMeshesOp::FInputMesh> InputMeshes;
	/** stash copies of the transforms and pointers to the meshes for consumption by the CSG Op*/
	void CacheInputMeshes();

	/** quickly generate a low-quality result for display while the actual result is being computed. */
	void CreateLowQualityPreview();

	void GenerateAsset(const FDynamicMeshOpResult& Result);
};
