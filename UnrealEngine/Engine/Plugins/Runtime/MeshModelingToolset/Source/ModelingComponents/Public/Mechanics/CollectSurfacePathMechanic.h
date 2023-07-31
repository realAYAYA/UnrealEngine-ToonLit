// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InteractionMechanic.h"
#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "ToolDataVisualizer.h"
#include "CollectSurfacePathMechanic.generated.h"

using UE::Geometry::FDynamicMesh3;

enum class ECollectSurfacePathDoneMode
{
	SnapCloseLoop,
	SnapDoubleClick,
	SnapDoubleClickOrCloseLoop,
	ExternalLambda,
	FixedNumPoints
};



/**
 */
UCLASS()
class MODELINGCOMPONENTS_API UCollectSurfacePathMechanic : public UInteractionMechanic
{
	GENERATED_BODY()
public:
	using FFrame3d = UE::Geometry::FFrame3d;

	TUniqueFunction<bool()> IsDoneFunc = nullptr;

	double ConstantSnapDistance = 10.0f;
	TUniqueFunction<bool(FVector3d, FVector3d)> SpatialSnapPointsFunc;

	bool bSnapToTargetMeshVertices = false;
	bool bSnapToWorldGrid = false;

	// tfunc to emit changes to...

	TArray<FFrame3d> HitPath;

	FFrame3d PreviewPathPoint;
	bool bPreviewPathPointValid = false;

	FToolDataVisualizer PathDrawer;
	FLinearColor PathColor;
	FLinearColor PreviewColor;
	FLinearColor PathCompleteColor;
	bool bDrawPath = true;


public:
	UCollectSurfacePathMechanic();

	virtual void Setup(UInteractiveTool* ParentTool) override;
	virtual void Shutdown() override;
	virtual void Render(IToolsContextRenderAPI* RenderAPI) override;

	/**
	 * Set the hit target mesh.
	 */
	virtual void InitializeMeshSurface(FDynamicMesh3&& TargetSurfaceMesh);
	virtual void InitializePlaneSurface(const FFrame3d& TargetPlane);

	virtual void SetFixedNumPointsMode(int32 NumPoints);
	virtual void SetDrawClosedLoopMode();
	virtual void SetCloseWithLambdaMode();
	virtual void SetDoubleClickOrCloseLoopMode();


	virtual bool IsHitByRay(const FRay3d& Ray, FFrame3d& HitPoint);
	virtual bool UpdatePreviewPoint(const FRay3d& Ray);
	virtual bool TryAddPointFromRay(const FRay3d& Ray);

	virtual bool PopLastPoint();

	virtual bool IsDone() const;

	/** Whether the path was finished by the user clicking on the first point */
	bool LoopWasClosed() const
	{
		return bLoopWasClosed;
	}

protected:
	FDynamicMesh3 TargetSurface;
	UE::Geometry::FDynamicMeshAABBTree3 TargetSurfaceAABB;

	FFrame3d TargetPlane;
	bool bHaveTargetPlane;

	bool RayToPathPoint(const FRay3d& Ray, FFrame3d& PointOut, bool bEnableSnapping);

	ECollectSurfacePathDoneMode DoneMode = ECollectSurfacePathDoneMode::SnapDoubleClick;
	int32 FixedPointTargetCount = 0;
	bool bCurrentPreviewWillComplete = false;
	bool bGeometricCloseOccurred = false;
	bool CheckGeometricClosure(const FFrame3d& Point, bool* bLoopWasClosedOut = nullptr);

	bool bLoopWasClosed = false;
};
