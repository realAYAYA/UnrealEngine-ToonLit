// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "CoreGlobals.h"
#include "HAL/IConsoleManager.h"
class UVEDITORTOOLS_API FUVEditorUXSettings
{
public:
	// The following are UV Editor specific style items that don't, strictly, matter to the SlateStyleSet,
	// but seemed appropriate to place here.

	// General Display Properties

	// The ScaleFactor just scales the mesh up. Scaling the mesh up makes it easier to zoom in
	// further into the display before getting issues with the camera near plane distance.
	static const float UVMeshScalingFactor;

	// Position to place the 2D camera far plane relative to world z
	static const float CameraFarPlaneWorldZ;

	// The near plane gets positioned some proportion to z = 0. We don't use a constant value because our depth offset values are percentage-based
	// Lower proportion move the plane nearer to world z
	// Note: This serves as an upper bound for all other depth offsets - higher than this value risks being clipped
	static const float CameraNearPlaneProportionZ;;


	// 2D Unwrap Display Properties
	static const float UnwrapTriangleOpacity;
	static const float UnwrapTriangleDepthOffset;
	static const float UnwrapTriangleOpacityWithBackground;

	static const float WireframeDepthOffset;
	static const float UnwrapBoundaryHueShift;
	static const float UnwrapBoundarySaturation;
	static const float UnwrapBoundaryValue;
	static const FColor UnwrapTriangleFillColor;
	static const FColor UnwrapTriangleWireframeColor;

	static FLinearColor GetTriangleColorByTargetIndex(int32 TargetIndex);
	static FLinearColor GetWireframeColorByTargetIndex(int32 TargetIndex);
	static FLinearColor GetBoundaryColorByTargetIndex(int32 TargetIndex);

	// Provides a color blindness friendly ramp for visualizing metrics and other data
	// Domain is [0 , 1]
	static FColor MakeCividisColorFromScalar(float Scalar);

	// Provides a color blindness friendly ramp for visualizing diverging metrics and other data
	// Domain is [-0.5, 0.5] 
	static FColor MakeTurboColorFromScalar(float Scalar);

	// Wireframe Properties
	static const float WireframeThickness;
	static const float BoundaryEdgeThickness;

	// Selection Highlighting Properties
	static const float SelectionTriangleOpacity;
	static const FColor SelectionTriangleFillColor;
	static const FColor SelectionTriangleWireframeColor;

	static const float SelectionHoverTriangleOpacity;
	static const FColor SelectionHoverTriangleFillColor;
	static const FColor SelectionHoverTriangleWireframeColor;

	static const float LivePreviewHighlightThickness;
	static const float LivePreviewHighlightPointSize;
	static const float LivePreviewHighlightDepthOffset;

	static const float SelectionLineThickness;
	static const float SelectionPointThickness;
	static const float SelectionWireframeDepthBias;
	static const float SelectionTriangleDepthBias;
	static const float SelectionHoverWireframeDepthBias;
	static const float SelectionHoverTriangleDepthBias;

	static const FColor LivePreviewExistingSeamColor;
	static const float LivePreviewExistingSeamThickness;
	static const float LivePreviewExistingSeamDepthBias;

	// These are currently used by the seam tool but can be generally used by
	// tools for displaying paths.
	static const FColor ToolLockedCutPathColor;
	static const FColor ToolLockedJoinPathColor;
	static const float ToolLockedPathThickness;
	static const float ToolLockedPathDepthBias;
	static const FColor ToolExtendCutPathColor;
	static const FColor ToolExtendJoinPathColor;
	static const float ToolExtendPathThickness;
	static const float ToolExtendPathDepthBias;
	static const FColor ToolCompletionPathColor;

	static const float ToolPointSize;

	// Sew Action styling
	static const float SewLineHighlightThickness;
	static const float SewLineDepthOffset;
	static const FColor SewSideLeftColor;
	static const FColor SewSideRightColor;

	// Grid 
	static const float AxisThickness;
	static const float GridMajorThickness;
	static const FColor XAxisColor;
	static const FColor YAxisColor;
	static const FColor RulerXColor;
	static const FColor RulerYColor;
	static const FColor GridMajorColor;
	static const FColor GridMinorColor;
	static const int32 GridSubdivisionsPerLevel;
	static const int32 GridLevels;
	static const int32 RulerSubdivisionLevel;

	// Pivots
	static const int32 PivotCircleNumSides;
	static const float PivotCircleRadius;
	static const float PivotLineThickness;
	static const FColor PivotLineColor;

	// Background
	static const float BackgroundQuadDepthOffset;

	//---------------------------
	//  Common Utility Methods
	//---------------------------


	// Note about the conversions from unwrap world to UV, below:
	// Nothing should make assumptions about the mapping between world space and UV coordinates. Instead,
	// tools should use the conversion functions below so that the mapping is free to change without affecting 
	// the tools.

	/**
	 * Converts from UV value as displayed to user or seen before import, to point in unwrap world space. This
	 * is useful for mapping visualizations to world unwrap space.
	 *
	 * Like other world-UV conversion functions, clients should not know about the details of this conversion.
	 */
	static FVector3d ExternalUVToUnwrapWorldPosition(const FVector2f& UV);

	/**
	 * Converts from point in unwrap world space, to UV value as displayed to user or seen before import. This
	 * is useful for labeling UV values in the unwrap world, even though the UV values are stored slightly differently
	 * inside the mesh itself.
	 *
	 * Like other world-UV conversion functions, clients should not know about the details of this conversion.
	 */
	static FVector2f UnwrapWorldPositionToExternalUV(const FVector3d& VertPosition);

	/**
	 * Converts from UV value as displayed to user or seen before import, to UV as stored on a mesh in Unreal.
	 */
	static FVector2f ExternalUVToInternalUV(const FVector2f& UV);

	/**
	 * Converts from UV as stored on a mesh in Unreal to UV as displayed to user or seen in an external program.
	 */
	static FVector2f InternalUVToExternalUV(const FVector2f& UV);

	/**
	 * Convert from UV as stored on a mesh in Unreal to world position in the UV editor unwrap world. This allows
	 * changes in UVs of a mesh to be mapped to its unwrap representation.
	 * 
	 * Like other world-UV conversion functions, clients should not know about the details of this conversion.
	 */
	static FVector3d UVToVertPosition(const FVector2f& UV);

	/**
	 * Converts from position in UV editor unwrap world to UV value as stored on a mesh in Unreal. This allows
	 * changes in the unwrap world to be mapped to actual mesh UVs.
	 * 
	 * Like other world-UV conversion functions, clients should not know about the details of this conversion.
	 */
	static FVector2f VertPositionToUV(const FVector3d& VertPosition);


	//--------------------------------
	// CVARs for Experimental Features
	//--------------------------------

	static TAutoConsoleVariable<int32> CVarEnablePrototypeUDIMSupport;

	//--------------------------------
	// Values for Snapping
	//--------------------------------
	static float LocationSnapValue(int32 LocationSnapMenuIndex);
	static int32 MaxLocationSnapValue();

private:

	FUVEditorUXSettings();
	~FUVEditorUXSettings();
};