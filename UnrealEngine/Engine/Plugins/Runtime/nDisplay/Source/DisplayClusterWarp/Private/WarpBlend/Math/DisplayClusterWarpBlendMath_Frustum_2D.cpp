// Copyright Epic Games, Inc. All Rights Reserved.

#include "WarpBlend/Math/DisplayClusterWarpBlendMath_Frustum.h"
#include "WarpBlend/Exporter/DisplayClusterWarpBlendExporter_WarpMap.h"
#include "WarpBlend/DisplayClusterWarpBlend.h"

#include "Render/Viewport/IDisplayClusterViewport.h"
#include "Render/Containers/IDisplayClusterRender_Texture.h"
#include "Misc/DisplayClusterHelpers.h"

//------------------------------------------------------------
// FDisplayClusterWarpBlendMath_Frustum
//------------------------------------------------------------
bool FDisplayClusterWarpBlendMath_Frustum::CalcFrustum2D()
{
	TRACE_CPUPROFILER_EVENT_SCOPE(DCWarpBlendMath_Frustum::CalcFrustum2D);

	// Calc projection
	return ImplCalcFrustumProjection() && EndCalcFrustum();
}
