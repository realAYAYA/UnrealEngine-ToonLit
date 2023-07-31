// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DynamicMesh/DynamicMesh3.h"
#include "DynamicMesh/DynamicMeshAttributeSet.h"


namespace UE
{
namespace Geometry
{

struct FUVOverlayView;
class FMeshConnectedComponents;

/**
 * FDynamicMeshUVPacker implements various strategies for packing UV islands in a 
 * UV Overlay. The island topology and UV unwraps must already be created, this
 * class simply scales/rotates/translates the islands to fit.
 */
class DYNAMICMESH_API FDynamicMeshUVPacker
{
public:
	/** The UV Overlay we will be repacking */
	FDynamicMeshUVOverlay* UVOverlay = nullptr;

	/** The explicit triangle ids to repack, repack all triangles if null */
	TUniquePtr<TArray<int32>> TidsToRepack;

	/** Resolution of the target texture. This is used to convert pixel gutter/border thickness to UV space */
	int32 TextureResolution = 512;

	/** Thickness of gutter/border in pixel dimensions. Not supported by all packing methods  */
	float GutterSize = 1.0;

	/** If true, islands can be flipped in addition to rotate/translate/scale */
	bool bAllowFlips = false;

	explicit FDynamicMeshUVPacker(FDynamicMeshUVOverlay* UVOverlay);
	explicit FDynamicMeshUVPacker(FDynamicMeshUVOverlay* UVOverlay, TUniquePtr<TArray<int32>>&& TidsToRepackIn);


	/**
	 * Standard UnrealEngine UV layout, similar to that used for Lightmap UVs. 
	 * All UV islands are packed into standard positive-unit-square.
	 * Only supports single-pixel border size.
	 */
	bool StandardPack();

	/**
	 * Uniformly scale all UV islands so that the largest fits in positive-unit-square,
	 * and translate each islands separately so that it's bbox-min is at the origin.
	 * So the islands are "stacked" and all fit in the unit box.
	 */
	bool StackPack();

protected:

	FMeshConnectedComponents CollectUVIslandsToPack(const FUVOverlayView& MeshView);

};


} // end namespace UE::Geometry
} // end namespace UE