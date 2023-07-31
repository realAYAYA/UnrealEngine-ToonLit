// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGizmos/GizmoElementCircleBase.h"
#include "InputState.h"
#include "UObject/ObjectMacros.h"
#include "GizmoElementArc.generated.h"

class FPrimitiveDrawInterface;
class FMaterialRenderProxy;

/**
 * Simple object intended to be used as part of 3D Gizmos.
 * Draws a thick arc based on parameters.
 */
UCLASS(Transient)
class INTERACTIVETOOLSFRAMEWORK_API UGizmoElementArc : public UGizmoElementCircleBase
{
	GENERATED_BODY()

public:

	//~ Begin UGizmoElementBase Interface.
	virtual void Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState) override;
	virtual FInputRayHit LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection) override;
	//~ End UGizmoElementBase Interface.

	// Inner circle radius.
	virtual void SetInnerRadius(double InInnerRadius);
	virtual double GetInnerRadius() const;

protected:

	// Arc inner radius.
	UPROPERTY()
	double InnerRadius = 5.0;
};