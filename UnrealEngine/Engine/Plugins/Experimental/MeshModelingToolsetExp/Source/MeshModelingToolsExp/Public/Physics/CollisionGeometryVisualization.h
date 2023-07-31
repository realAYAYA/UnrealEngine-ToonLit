// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Physics/PhysicsDataCollection.h"
#include "Drawing/PreviewGeometryActor.h"

namespace UE
{
	namespace PhysicsTools
	{
		/**
		 * Create line sets in a UPreviewGeometry for all the elements in a Physics Data Collection.
		 * Spheres and Capsules are drawn as 3-axis wireframes. Convexes are added as wireframes.
		 */
		void MESHMODELINGTOOLSEXP_API InitializePreviewGeometryLines(const FPhysicsDataCollection& PhysicsData, UPreviewGeometry* PreviewGeom,
			const FColor& LineColor, float LineThickness, float DepthBias = 0.0, int32 CircleStepResolution = 16, bool bRandomColors = true );
	}
}


