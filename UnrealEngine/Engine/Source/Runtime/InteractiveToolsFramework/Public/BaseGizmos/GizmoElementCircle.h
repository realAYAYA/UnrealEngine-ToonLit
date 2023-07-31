// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGizmos/GizmoElementCircleBase.h"
#include "InputState.h"
#include "UObject/ObjectMacros.h"
#include "GizmoElementCircle.generated.h"

/**
 * Simple object intended to be used as part of 3D Gizmos.
 * Draws a filled or line circle based on parameters.
 * 
 * The circle element does not yet support partial circles.
 */
UCLASS(Transient)
class INTERACTIVETOOLSFRAMEWORK_API UGizmoElementCircle : public UGizmoElementCircleBase
{
	GENERATED_BODY()

public:
	//~ Begin UGizmoElementBase Interface.
	virtual void Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState) override;
	virtual FInputRayHit LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection) override;
	//~ End UGizmoElementBase Interface.

	// Draw mesh
	virtual void SetDrawMesh(bool InDrawMesh);
	virtual bool GetDrawMesh() const;

	// Draw line
	virtual void SetDrawLine(bool InDrawLine);
	virtual bool GetDrawLine() const;

	// Hit mesh
	virtual void SetHitMesh(bool InHitMesh);
	virtual bool GetHitMesh() const;

	// Hit line
	virtual void SetHitLine(bool InHitLine);
	virtual bool GetHitLine() const;

protected:

	// Whether to render solid circle.
	UPROPERTY()
	bool bDrawMesh = true;

	// Whether to render line circle.
	UPROPERTY()
	bool bDrawLine = false;

	// Whether to perform hit test on mesh.
	UPROPERTY()
	bool bHitMesh = true;

	// Whether to perform hit test on line.
	UPROPERTY()
	bool bHitLine = false;
};