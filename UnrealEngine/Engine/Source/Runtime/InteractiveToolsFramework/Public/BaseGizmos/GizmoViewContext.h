// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "SceneView.h"

#include "GizmoViewContext.generated.h"

/** 
 * A context object that is meant to hold the scene information for the hovered viewport
 * on a game thread, to be used by a gizmo later for hit testing. The organization mirrors
 * FSceneView so that functions could be written in a templated way to use either FSceneView
 * or UGizmoViewContext, though UGizmoViewContext only keeps the needed data.
 */
UCLASS(MinimalAPI)
class UGizmoViewContext : public UObject
{
	GENERATED_BODY()
public:
	
	// Wrapping class for the matrices so that they can be accessed in the same way
	// that they are accessed in FSceneView.
	class FMatrices
	{
	public:
		void ResetFromSceneView(const FSceneView& SceneView)
		{
			ViewMatrix = SceneView.ViewMatrices.GetViewMatrix();
			InvViewMatrix = SceneView.ViewMatrices.GetInvViewMatrix();
			ViewProjectionMatrix = SceneView.ViewMatrices.GetViewProjectionMatrix();
		}

		const FMatrix& GetViewMatrix() const { return ViewMatrix; }
		const FMatrix& GetInvViewMatrix() const { return InvViewMatrix; }
		const FMatrix& GetViewProjectionMatrix() const { return ViewProjectionMatrix; }

	protected:
		FMatrix ViewMatrix;
		FMatrix InvViewMatrix;
		FMatrix ViewProjectionMatrix;
	};

	// Use this to reinitialize the object each frame for the hovered viewport.
	void ResetFromSceneView(const FSceneView& SceneView)
	{
		UnscaledViewRect = SceneView.UnscaledViewRect;
		ViewMatrices.ResetFromSceneView(SceneView);
		bIsPerspectiveProjection = SceneView.IsPerspectiveProjection();
		ViewLocation = SceneView.ViewLocation;
	}

	// FSceneView-like functions/properties:
	FVector GetViewRight() const { return ViewMatrices.GetViewMatrix().GetColumn(0); }
	FVector GetViewUp() const { return ViewMatrices.GetViewMatrix().GetColumn(1); }
	FVector GetViewDirection() const { return ViewMatrices.GetViewMatrix().GetColumn(2); }

	// As a function just for similarity with FSceneView
	bool IsPerspectiveProjection() const { return bIsPerspectiveProjection; }

	FVector4 WorldToScreen(const FVector& WorldPoint) const
	{
		return ViewMatrices.GetViewProjectionMatrix().TransformFVector4(FVector4(WorldPoint, 1));
	}

	bool ScreenToPixel(const FVector4& ScreenPoint, FVector2D& OutPixelLocation) const
	{
		// implementation copied from FSceneView::ScreenToPixel() 

		if (ScreenPoint.W != 0.0f)
		{
			//Reverse the W in the case it is negative, this allow to manipulate a manipulator in the same direction when the camera is really close to the manipulator.
			double InvW = (ScreenPoint.W > 0.0 ? 1.0 : -1.0) / ScreenPoint.W;
			double Y = (GProjectionSignY > 0.0) ? ScreenPoint.Y : 1.0 - ScreenPoint.Y;
			OutPixelLocation = FVector2D(
				UnscaledViewRect.Min.X + (0.5 + ScreenPoint.X * 0.5 * InvW) * UnscaledViewRect.Width(),
				UnscaledViewRect.Min.Y + (0.5 - Y * 0.5 * InvW) * UnscaledViewRect.Height()
			);
			return true;
		}
		else
		{
			return false;
		}
	}

	FMatrices ViewMatrices;
	FIntRect UnscaledViewRect;
	FVector	ViewLocation;

protected:
	bool bIsPerspectiveProjection;
};
