// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/PrimitiveComponent.h"
#include "SceneView.h"

class UGizmoViewContext;

/**
 * Utility functions for standard GizmoComponent rendering
 */
namespace GizmoRenderingUtil
{

	/**
	 * @return Conversion factor between pixel and world-space coordinates at 3D point Location in View.
	 * @warning This is a local estimate and is increasingly incorrect as the 3D point gets further from Location
	 */
	INTERACTIVETOOLSFRAMEWORK_API float CalculateLocalPixelToWorldScale(
		const FSceneView* View,
		const FVector& Location);
	INTERACTIVETOOLSFRAMEWORK_API float CalculateLocalPixelToWorldScale(
		const UGizmoViewContext* ViewContext,
		const FVector& Location);

	/**
	 * @return Conversion factor between pixel and world-space coordinates at 3D point Location in View.
	 * @warning This is a local estimate and is increasingly incorrect as the 3D point gets further from Location
	 */
	INTERACTIVETOOLSFRAMEWORK_API float CalculateLocalPixelToWorldScale(
		const FSceneView* View,
		const FVector& Location);

	/**
	 * @return Legacy view dependent conversion factor.
	 * @return OutWorldFlattenScale vector to be applied in world space, can be used to flatten excluded 
	 *         dimension in orthographic views as it reverses the scale in that dimension.
	 */
	INTERACTIVETOOLSFRAMEWORK_API float CalculateViewDependentScaleAndFlatten(
		const FSceneView* View,
		const FVector& Location,
		const float Scale,
		FVector& OutWorldFlattenScale);
}
