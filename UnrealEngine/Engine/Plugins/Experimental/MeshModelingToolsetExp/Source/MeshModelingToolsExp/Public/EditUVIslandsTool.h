// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BaseTools/MeshSurfacePointMeshEditingTool.h"
#include "Changes/MeshVertexChange.h"
#include "Components/DynamicMeshComponent.h"
#include "CoreMinimal.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "GroupTopology.h"
#include "ModelingTaskTypes.h"
#include "Properties/MeshMaterialProperties.h"
#include "Selection/GroupTopologySelector.h"
#include "Selection/PolygonSelectionMechanic.h"
#include "ToolDataVisualizer.h"
#include "Transforms/MultiTransformer.h"
#include "EditUVIslandsTool.generated.h"

class FMeshVertexChangeBuilder;
using UE::Geometry::FDynamicMeshUVOverlay;

/**
 * ToolBuilder
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UEditUVIslandsToolBuilder : public UMeshSurfacePointMeshEditingToolBuilder
{
	GENERATED_BODY()
public:
	virtual UMeshSurfacePointTool* CreateNewTool(const FToolBuilderState& SceneState) const override;
};




class MESHMODELINGTOOLSEXP_API FUVGroupTopology : public FGroupTopology
{
public:
	TArray<int32> TriIslandGroups;
	const FDynamicMeshUVOverlay* UVOverlay;

	FUVGroupTopology() {}
	FUVGroupTopology(const UE::Geometry::FDynamicMesh3* Mesh, uint32 UVLayerIndex, bool bAutoBuild = false);

	void CalculateIslandGroups();

	virtual int GetGroupID(int32 TriangleID) const override
	{
		return TriIslandGroups[TriangleID];
	}

	UE::Geometry::FFrame3d GetIslandFrame(int32 GroupID, UE::Geometry::FDynamicMeshAABBTree3& AABBTree);
};


/**
 *
 */
UCLASS()
class MESHMODELINGTOOLSEXP_API UEditUVIslandsTool : public UMeshSurfacePointTool
{
	GENERATED_BODY()

public:
	UEditUVIslandsTool();

	virtual void RegisterActions(FInteractiveToolActionSet& ActionSet) override;

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual void OnTick(float DeltaTime) override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	virtual bool HasCancel() const override { return true; }
	virtual bool HasAccept() const override { return true; }

	// UMeshSurfacePointTool API
	virtual bool HitTest(const FRay& Ray, FHitResult& OutHit) override;
	virtual void OnBeginDrag(const FRay& Ray) override;
	virtual void OnUpdateDrag(const FRay& Ray) override;
	virtual void OnEndDrag(const FRay& Ray) override;
	virtual bool OnUpdateHover(const FInputDeviceRay& DevicePos) override;
	virtual void OnEndHover() override;

	// IClickDragBehaviorTarget API
	virtual FInputRayHit CanBeginClickDragSequence(const FInputDeviceRay& PressPos) override;


public:
	UPROPERTY()
	TObjectPtr<UExistingMeshMaterialProperties> MaterialSettings = nullptr;

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> CheckerMaterial = nullptr;

protected:

	UPROPERTY()
	TObjectPtr<AInternalToolFrameworkActor> PreviewMeshActor = nullptr;

	UPROPERTY()
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent = nullptr;

	UPROPERTY()
	TObjectPtr<UPolygonSelectionMechanic> SelectionMechanic;

	bool bSelectionStateDirty = false;
	void OnSelectionModifiedEvent();

	UPROPERTY()
	TObjectPtr<UMultiTransformer> MultiTransformer = nullptr;

	void OnMultiTransformerTransformBegin();
	void OnMultiTransformerTransformUpdate();
	void OnMultiTransformerTransformEnd();

	// realtime visualization
	void OnDynamicMeshComponentChanged();
	FDelegateHandle OnDynamicMeshComponentChangedHandle;

	// camera state at last render
	FTransform3d WorldTransform;
	FViewCameraState CameraState;

	// True for the duration of UI click+drag
	bool bInDrag;

	double UVTranslateScale;
	UE::Geometry::FFrame3d InitialGizmoFrame;
	FVector3d InitialGizmoScale;
	void ComputeUpdate_Gizmo();

	FUVGroupTopology Topology;
	void PrecomputeTopology();

	FDynamicMeshAABBTree3 MeshSpatial;
	FDynamicMeshAABBTree3& GetSpatial();
	bool bSpatialDirty;


	//
	// data for current drag
	//
	struct FEditIsland
	{
		UE::Geometry::FFrame3d LocalFrame;
		TArray<int32> Triangles;
		TArray<int32> UVs;
		UE::Geometry::FAxisAlignedBox2d UVBounds;
		FVector2d UVOrigin;
		TArray<FVector2f> InitialPositions;
	};
	TArray<FEditIsland> ActiveIslands;
	void UpdateUVTransformFromSelection(const FGroupTopologySelection& Selection);

	FMeshVertexChangeBuilder* ActiveVertexChange;
	void BeginChange();
	void EndChange();
	void UpdateChangeFromROI(bool bFinal);

	void OnMaterialSettingsChanged();
};

