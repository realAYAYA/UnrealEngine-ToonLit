// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BoneContainer.h"
#include "InteractiveToolBuilder.h"
#include "ModelingOperators.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"

#include "SkinWeightsBindingTool.generated.h"

class UMeshOpPreviewWithBackgroundCompute;
struct FDynamicMeshOpResult;
struct FMeshDescription;
class IMeshDescriptionCommitter;
struct FOccupancyGrid;

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API USkinWeightsBindingToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};

UENUM()
enum class ESkinWeightsBindType : uint8
{
	DirectDistance = 0 UMETA(DisplayName = "Direct Distance"),
	// GeodesicDistance UMETA(DisplayName = "Geodesic Distance"),
	// Heatmaps UMETA(DisplayName = "Heatmap"),
	GeodesicVoxel = 3 UMETA(DisplayName = "Geodesic Voxel"),
};

UCLASS()
class MESHMODELINGTOOLSEXP_API USkinWeightsBindingToolProperties : 
	public UInteractiveToolPropertySet,
	public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()
public:
	UPROPERTY(VisibleAnywhere, Category = Skeleton)
	FBoneReference CurrentBone;

	/** Binding type to use */
	UPROPERTY(EditAnywhere, Category = Binding)
	ESkinWeightsBindType BindingType = ESkinWeightsBindType::DirectDistance;

	/** Stiffness of binding. Lower values allow more distant bones to contribute more */
	UPROPERTY(EditAnywhere, Category = Binding)
	float Stiffness = 0.2f;

	/** Maximum bones that will influence each vertex */
	UPROPERTY(EditAnywhere, Category = Binding, meta=(ClampMin="1", UIMin="1", UIMax="10"))
	int32 MaxInfluences = 5;

	/** The resolution of the voxel grid if doing geodesic voxel binding */
	UPROPERTY(EditAnywhere, Category = Binding, meta=(EditCondition = "BindingType == ESkinWeightsBindType::GeodesicVoxel", ClampMin="1", UIMin="8", UIMax="1024"))
	int32 VoxelResolution = 128;
	
	// UPROPERTY(EditAnywhere, Category = Debug)
	bool bDebugDraw = false;
	
	// IBoneReferenceSkeletonProvider
	USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;

	TObjectPtr<USkeletalMesh> SkeletalMesh;
};

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API USkinWeightsBindingTool :
	public UMultiSelectionMeshEditingTool,
	public UE::Geometry::IDynamicMeshOperatorFactory
{
	GENERATED_BODY()

public:
	USkinWeightsBindingTool();
	~USkinWeightsBindingTool();
	
	void Setup() override;
	void OnShutdown(EToolShutdownType ShutdownType) override;
	
	void OnTick(float DeltaTime) override;
	void Render(IToolsContextRenderAPI* RenderAPI) override;

	bool HasCancel() const override { return true; }
	bool HasAccept() const override { return true; }
	bool CanAccept() const override;

	void OnPropertyModified(UObject* PropertySet, FProperty* Property) override;

	// IDynamicMeshOperatorFactory API
	TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<USkinWeightsBindingToolProperties> Properties = nullptr;
	
	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> Preview = nullptr;

protected:
	TSharedPtr<FOccupancyGrid> Occupancy;
	
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	
	FBoneContainer BoneContainer;
	TMap<FName, FBoneIndexType> BoneToIndex;
	TArray<TPair<FTransform, int32>> TransformHierarchy;

	void GenerateAsset(const FDynamicMeshOpResult& Result);

	static FVector4f WeightToColor(float Value);
	void UpdateVisualization(bool bInForce = false);
};
