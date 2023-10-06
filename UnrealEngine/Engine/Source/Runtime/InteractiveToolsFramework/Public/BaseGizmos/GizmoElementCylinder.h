// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGizmos/GizmoElementBase.h"
#include "InputState.h"
#include "UObject/ObjectMacros.h"
#include "GizmoElementCylinder.generated.h"

/**
 * Simple object intended to be used as part of 3D Gizmos.
 * Draws a solid 3D cylinder based on parameters.
 */

UCLASS(Transient, MinimalAPI)
class UGizmoElementCylinder : public UGizmoElementBase
{
	GENERATED_BODY()

public:

	//~ Begin UGizmoElementBase Interface.
	INTERACTIVETOOLSFRAMEWORK_API virtual void Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState) override;
	INTERACTIVETOOLSFRAMEWORK_API virtual FInputRayHit LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection) override;
	//~ End UGizmoElementBase Interface.

	// Location of center of cylinder's base circle.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetBase(const FVector& InBase);
	INTERACTIVETOOLSFRAMEWORK_API virtual FVector GetBase() const;

	// Cylinder axis direction.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetDirection(const FVector& InDirection);
	INTERACTIVETOOLSFRAMEWORK_API virtual FVector GetDirection() const;

	// Cylinder height.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetHeight(float InHeight);
	INTERACTIVETOOLSFRAMEWORK_API virtual float GetHeight() const;

	// Cylinder radius.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetRadius(float InRadius);
	INTERACTIVETOOLSFRAMEWORK_API virtual float GetRadius() const;

	// Number of sides for tessellating cylinder.
	INTERACTIVETOOLSFRAMEWORK_API virtual void SetNumSides(int32 InNumSides);
	INTERACTIVETOOLSFRAMEWORK_API virtual int32 GetNumSides() const;

protected:

	// Location of center of cylinder's base circle.
	UPROPERTY()
	FVector Base = FVector::ZeroVector;

	// Cylinder axis direction.
	UPROPERTY()
	FVector Direction = FVector(0.0f, 0.0f, 1.0f);

	// Cylinder height.
	UPROPERTY()
	float Height = 1.0f;

	// Cylinder radius.
	UPROPERTY()
	float Radius = 0.5f;

	// Number of sides for tessellating cylinder.
	UPROPERTY()
	int32 NumSides = 32;
};

