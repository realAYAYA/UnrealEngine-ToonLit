// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h" // Required by MESHMODELINGTOOLSEXP_API

class FPhysicsDataCollection;
class UCollisionGeometryVisualizationProperties;
class UPreviewGeometry;
class UMaterialInterface;

namespace UE::PhysicsTools
{
	/**
	 * Create line sets in a UPreviewGeometry for all the elements in a Physics Data Collection.
	 * Spheres and Capsules are drawn as 3-axis wireframes. Convexes are added as wireframes.
	 */
	void MESHMODELINGTOOLSEXP_API InitializeCollisionGeometryVisualization(
		UPreviewGeometry* PreviewGeom,
		UCollisionGeometryVisualizationProperties* Settings,
		const FPhysicsDataCollection& PhysicsData,
		float DepthBias = 0.f,
		int32 CircleStepResolution = 16);

	/**
	 * Update line sets in a UPreviewGeometry.
	 */
	void MESHMODELINGTOOLSEXP_API UpdateCollisionGeometryVisualization(
		UPreviewGeometry* PreviewGeom,
		UCollisionGeometryVisualizationProperties* Settings);

	// The following functions are like InitializeCollisionGeometryVisualization/UpdateCollisionGeometryVisualization
	// but assume that the given UPreviewGeometry only represents part of the complete collision geometry visualization

	/**
	 * Like InitializeCollisionGeometryVisualization but only initializes part of the collision geometry visualization.
	 * 
	 * The FirstLineSetIndex parameter is used implement Settings->bRandomColors since random colors are determined
	 * using unique integers for each line set, which may be stored in separate UPreviewGeometry objects. Since the
	 * initialization is partial this function doesn't set Settings->bVisualizationDirty, the caller should do that.
	 */
	void MESHMODELINGTOOLSEXP_API PartiallyInitializeCollisionGeometryVisualization(
		UPreviewGeometry* PartialPreviewGeom,
		UCollisionGeometryVisualizationProperties* Settings,
		const FPhysicsDataCollection& PhysicsData,
		int32 FirstLineSetIndex,
		float DepthBias = 0.f,
		int32 CircleStepResolution = 16);

	/**
	 * Like UpdateCollisionGeometryVisualization but only updates part of the collision geometry visualization.
	 * 
	 * The FirstLineSetIndex parameter is used implement Settings->bRandomColors since random colors are determined
	 * using unique integers for each line set, which may be stored in separate UPreviewGeometry objects. Since the
	 * update is partial this function doesn't check or set Settings->bVisualizationDirty, the caller should do that. 
	 */
	void MESHMODELINGTOOLSEXP_API PartiallyUpdateCollisionGeometryVisualization(
		UPreviewGeometry* PartialPreviewGeom,
		UCollisionGeometryVisualizationProperties* Settings,
		int32 FirstLineSetIndex);
}


