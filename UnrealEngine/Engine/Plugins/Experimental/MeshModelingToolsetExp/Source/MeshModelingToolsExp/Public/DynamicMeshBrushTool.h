// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "InteractiveToolBuilder.h"
#include "BaseTools/BaseBrushTool.h"
#include "DynamicMesh/DynamicMeshAABBTree3.h"
#include "Components/DynamicMeshComponent.h"
#include "PreviewMesh.h"
#include "DynamicMeshBrushTool.generated.h"

PREDECLARE_USE_GEOMETRY_CLASS(FDynamicMesh3);

/**
 * UDynamicMeshBrushTool is a base class that specializes UBaseBrushTool
 * for brushing on an FDynamicMesh3. The input FPrimitiveComponentTarget is hidden
 * and a UPreviewMesh is created and shown in its place. This UPreviewMesh is
 * used for hit-testing and dynamic rendering.
 * 
 */
UCLASS(Transient)
class MESHMODELINGTOOLSEXP_API UDynamicMeshBrushTool : public UBaseBrushTool
{
	GENERATED_BODY()

public:
	UDynamicMeshBrushTool();

	// UInteractiveTool API

	virtual void Setup() override;
	virtual void Shutdown(EToolShutdownType ShutdownType) override;

	virtual bool HitTest(const FRay& Ray, FHitResult& OutHit) override;

protected:
	// subclasses can override these to customize behavior
	virtual void OnShutdown(EToolShutdownType ShutdownType) {}


protected:
	UPROPERTY()
	TObjectPtr<UPreviewMesh> PreviewMesh;

	// this function is called when the component inside the PreviewMesh is modified (eg via an undo/redo event)
	virtual void OnBaseMeshComponentChanged() {}	
	FDelegateHandle OnBaseMeshComponentChangedHandle;

	UE::Geometry::FAxisAlignedBox3d InputMeshBoundsLocal;

	//
	// UBaseBrushTool private interface
	//
	virtual double EstimateMaximumTargetDimension() override;
};