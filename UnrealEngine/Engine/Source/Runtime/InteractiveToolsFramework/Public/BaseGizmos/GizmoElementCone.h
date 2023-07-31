// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGizmos/GizmoElementBase.h"
#include "InputState.h"
#include "UObject/ObjectMacros.h"
#include "GizmoElementCone.generated.h"

/**
 * Simple object intended to be used as part of 3D Gizmos.
 * Draws a solid 3D cone based on parameters.
 */

UCLASS(Transient)
class INTERACTIVETOOLSFRAMEWORK_API UGizmoElementCone : public UGizmoElementBase
{
	GENERATED_BODY()

public:

	//~ Begin UGizmoElementBase Interface.
	virtual void Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState) override;
	virtual FInputRayHit LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection) override;
	//~ End UGizmoElementBase Interface.

	// Cone tip location.
	virtual void SetOrigin(const FVector& InBase);
	virtual FVector GetOrigin() const;

	// Cone axis direction pointing from tip toward base of cone.
	virtual void SetDirection(const FVector& InDirection);
	virtual FVector GetDirection() const;

	// Cone height.
	virtual void SetHeight(float InHeight);
	virtual float GetHeight() const;

	// Cone radius.
	virtual void SetRadius(float InRadius);
	virtual float GetRadius() const;

	// Number of sides for tessellating cone.
	virtual void SetNumSides(int32 InNumSides);
	virtual int32 GetNumSides() const;

protected:

	// Cone tip location.
	UPROPERTY()
	FVector Origin = FVector::ZeroVector;

	// Cone axis direction pointing from tip toward base of cone.
	UPROPERTY()
	FVector Direction = FVector(0.0f, 0.0f, -1.0f);

	// Cone height.
	UPROPERTY()
	float Height = 1.0f;

	// Cone radius.
	UPROPERTY()
	float Radius = 0.5f;

	// Number of sides for tessellating cone.
	UPROPERTY()
	int32 NumSides = 32;
};

