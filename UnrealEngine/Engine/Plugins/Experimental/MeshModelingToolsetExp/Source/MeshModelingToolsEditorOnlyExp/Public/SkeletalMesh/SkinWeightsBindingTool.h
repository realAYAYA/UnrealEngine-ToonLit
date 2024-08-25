// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "BoneContainer.h"
#include "InteractiveToolBuilder.h"
#include "ModelingOperators.h"
#include "SkeletalMeshEditionInterface.h"
#include "BaseTools/MultiSelectionMeshEditingTool.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"

#include "SkinWeightsBindingTool.generated.h"

class UMeshOpPreviewWithBackgroundCompute;
struct FDynamicMeshOpResult;
struct FMeshDescription;
class IMeshDescriptionCommitter;
struct FOccupancyGrid;

namespace UE::Geometry
{
	struct FOccupancyGrid3;
}

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USkinWeightsBindingToolBuilder : public UMultiSelectionMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual bool CanBuildTool(const FToolBuilderState& SceneState) const override;
	virtual UMultiSelectionMeshEditingTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};

// A mirror of UE::Geometry::ESkinBindingType
UENUM()
enum class ESkinWeightsBindType : uint8
{
	DirectDistance = 0 UMETA(DisplayName = "Direct Distance"),
	GeodesicVoxel = 1 UMETA(DisplayName = "Geodesic Voxel"),
};

UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USkinWeightsBindingToolProperties : 
	public UInteractiveToolPropertySet
{
	GENERATED_BODY()
public:
	UPROPERTY()
	FName CurrentBone = NAME_None;

	/** Binding type to use */
	UPROPERTY(EditAnywhere, Category = Binding)
	ESkinWeightsBindType BindingType = ESkinWeightsBindType::DirectDistance;

	/** Stiffness of binding. Lower values allow more distant bones to contribute more */
	UPROPERTY(EditAnywhere, Category = Binding, meta=(ClampMin="0.0", ClampMax="1.0", UIMin="0.0", UIMax="1.0"))
	float Stiffness = 0.2f;

	/** Maximum bones that will influence each vertex */
	UPROPERTY(EditAnywhere, Category = Binding, meta=(ClampMin="1", UIMin="1", UIMax="10"))
	int32 MaxInfluences = 5;

	/** The resolution of the voxel grid if doing geodesic voxel binding */
	UPROPERTY(EditAnywhere, Category = Binding, meta=(EditCondition = "BindingType == ESkinWeightsBindType::GeodesicVoxel", ClampMin="1", UIMin="8", UIMax="1024"))
	int32 VoxelResolution = 128;
	
	// UPROPERTY(EditAnywhere, Category = Debug)
	bool bDebugDraw = false;
};

/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEDITORONLYEXP_API USkinWeightsBindingTool :
	public UMultiSelectionMeshEditingTool,
	public UE::Geometry::IDynamicMeshOperatorFactory,
	public ISkeletalMeshEditingInterface
{
	GENERATED_BODY()

public:
	USkinWeightsBindingTool();
	virtual ~USkinWeightsBindingTool() override;

	void Init(const FToolBuilderState& InSceneState);
	
	virtual void Setup() override;
	virtual void OnShutdown(EToolShutdownType ShutdownType) override;
	
	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }
	virtual bool CanAccept() const override;

	// IDynamicMeshOperatorFactory API
	virtual TUniquePtr<UE::Geometry::FDynamicMeshOperator> MakeNewOperator() override;

	UPROPERTY()
	TObjectPtr<USkinWeightsBindingToolProperties> Properties = nullptr;
	
	UPROPERTY()
	TObjectPtr<UMeshOpPreviewWithBackgroundCompute> Preview = nullptr;

protected:
	TSharedPtr<UE::Geometry::FOccupancyGrid3> Occupancy;
	
	TSharedPtr<UE::Geometry::FDynamicMesh3, ESPMode::ThreadSafe> OriginalMesh;
	
	TUniquePtr<FMeshDescription> EditedMeshDescription;

	FReferenceSkeleton ReferenceSkeleton;
	
	void GenerateAsset(const FDynamicMeshOpResult& Result);

	static FVector4f WeightToColor(float Value);
	void UpdateVisualization(bool bInForce = false);

	UPROPERTY()
	TWeakObjectPtr<USkeletalMeshEditorContextObjectBase> EditorContext = nullptr;

	// updates the skin weights using the operator's result
	bool UpdateSkinWeightsFromDynamicMesh(UE::Geometry::FDynamicMesh3& InResultMesh) const;
	
	// ISkeletalMeshEditionInterface
	virtual void HandleSkeletalMeshModified(const TArray<FName>& InBoneNames, const ESkeletalMeshNotifyType InNotifyType) override;
};
