// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BaseGizmos/GizmoElementBase.h"
#include "InputState.h"
#include "UObject/ObjectMacros.h"
#include "GizmoElementBox.generated.h"

/**
 * Simple object intended to be used as part of 3D Gizmos.
 * Draws a solid 3D cylinder based on parameters.
 */

UCLASS(Transient)
class INTERACTIVETOOLSFRAMEWORK_API UGizmoElementBox : public UGizmoElementBase
{
	GENERATED_BODY()

public:

	//~ Begin UGizmoElementBase Interface.
	virtual void Render(IToolsContextRenderAPI* RenderAPI, const FRenderTraversalState& RenderState) override;
	virtual FInputRayHit LineTrace(const UGizmoViewContext* ViewContext, const FLineTraceTraversalState& LineTraceState, const FVector& RayOrigin, const FVector& RayDirection) override;
	//~ End UGizmoElementBase Interface.

	// Location of center of box's base circle.
	virtual void SetCenter(const FVector& InCenter);
	virtual FVector GetCenter() const;

	// Box axis up direction.
	virtual void SetUpDirection(const FVector& InUpDirection);
	virtual FVector GetUpDirection() const;

	// Box axis side direction.
	virtual void SetSideDirection(const FVector& InSideDirection);
	virtual FVector GetSideDirection() const;

	// Box dimensions.
	virtual void SetDimensions(const FVector& InDimensions);
	virtual FVector GetDimensions() const;

protected:

	// Location of center of box
	UPROPERTY()
	FVector Center = FVector::ZeroVector;

	// Box dimensions
	UPROPERTY()
	FVector Dimensions = FVector(1.0f, 1.0f, 1.0f);

	// Box up axis, corresponds to Z dimension
	UPROPERTY()
	FVector UpDirection = FVector::UpVector;

	// Box side axis, corresponds to Y dimension
	UPROPERTY()
	FVector SideDirection = FVector::RightVector;
};

