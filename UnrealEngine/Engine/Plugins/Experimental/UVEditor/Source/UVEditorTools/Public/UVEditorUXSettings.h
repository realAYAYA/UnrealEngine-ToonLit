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
	static const FColor ToolLockedPathColor;
	static const float ToolLockedPathThickness;
	static const float ToolLockedPathDepthBias;
	static const FColor ToolExtendPathColor;
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


	// When creating UV unwraps or transforming between UV coordinate and positions in the 2D viewport,
	// these functions will determine the mapping between UV values and the
	// resulting unwrap mesh vertex positions. 
	// If we're looking down on the unwrapped mesh, with the Z axis towards us, we want U's to be right, and
	// V's to be up. In Unreal's left-handed coordinate system, this means that we map U's to world Y
	// and V's to world X.
	// Also, Unreal changes the V coordinates of imported meshes to 1-V internally, and we undo this
	// while displaying the UV's because the users likely expect to see the original UV's (it would
	// be particularly confusing for users working with UDIM assets, where internally stored V's 
	// frequently end up negative).
	// The ScaleFactor just scales the mesh up. Scaling the mesh up makes it easier to zoom in
	// further into the display before getting issues with the camera near plane distance.
	static FVector2f ExternalUVToInternalUV(const FVector2f& UV);

	static FVector2f InternalUVToExternalUV(const FVector2f& UV);

	static FVector3d UVToVertPosition(const FVector2f& UV);

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