// Copyright Epic Games, Inc. All Rights Reserved.

#include "WarpBlend/DisplayClusterWarpBlend.h"
#include "WarpBlend/Math/DisplayClusterWarpBlendMath_Frustum.h"

// Select mpcdi frustum calc method
static TAutoConsoleVariable<int32> CVarMPCDIFrustumMethod(
	TEXT("nDisplay.render.mpcdi.Frustum"),
	(int32)EDisplayClusterWarpBlendFrustumType::LOD,
	TEXT("Frustum computation method:\n")
	TEXT(" 0: mesh AABB based, lower quality but fast\n")
	TEXT(" 1: mesh vertices based, best quality but slow\n")
	TEXT(" 2: LOD, get A*B distributed points from texture, fast, good quality for flat panels\n"),
	ECVF_RenderThreadSafe
);

// Select mpcdi stereo mode
static TAutoConsoleVariable<int32> CVarMPCDIStereoMode(
	TEXT("nDisplay.render.mpcdi.StereoMode"),
	(int32)EDisplayClusterWarpBlendStereoMode::AsymmetricAABB,
	TEXT("Stereo mode:\n")
	TEXT(" 0: Asymmetric to AABB center\n")
	TEXT(" 1: Symmetric to AABB center\n"),
	ECVF_RenderThreadSafe
);

// Select mpcdi projection mode
static TAutoConsoleVariable<int32> CVarMPCDIProjectionMode(
	TEXT("nDisplay.render.mpcdi.Projection"),
	(int32)EDisplayClusterWarpBlendProjectionType::StaticSurfaceNormal,
	TEXT("Projection method:\n")
	TEXT(" 0: Static, aligned to average region surface normal\n")
	TEXT(" 1: Static, aligned to average region surface corners plane\n")
	TEXT(" 2: Dynamic, to view target center\n"),
	ECVF_RenderThreadSafe
);

// Frustum projection fix (back-side view planes)
static TAutoConsoleVariable<int32> CVarMPCDIProjectionAuto(
	TEXT("nDisplay.render.mpcdi.ProjectionAuto"),
	1, // Default on
	TEXT("Runtime frustum method, fix back-side view projection.\n")
	TEXT(" 0: Disabled\n")
	TEXT(" 1: Enabled (default)\n"),
	ECVF_RenderThreadSafe
);

// Setup frustum projection cache
static TAutoConsoleVariable<int32> CVarMPCDIFrustumCacheDepth(
	TEXT("nDisplay.render.mpcdi.cache_depth"),
	0,// Default disabled
	TEXT("Frustum values cache (depth, num).\n")
	TEXT("By default cache is disabled. For better performance (EDisplayClusterWarpBlendFrustumType::FULL) set value to 512).\n")
	TEXT(" 0: Disabled\n")
	TEXT(" N: Cache size, integer\n"),
	ECVF_RenderThreadSafe
);

static TAutoConsoleVariable<float> CVarMPCDIFrustumCachePrecision(
	TEXT("nDisplay.render.mpcdi.cache_precision"),
	0.1f, // 1mm
	TEXT("Frustum cache values comparison precision (float, unit is sm).\n"),
	ECVF_RenderThreadSafe
);

bool FDisplayClusterWarpBlend::MarkWarpGeometryComponentDirty(const FName& InComponentName)
{
	return GeometryContext.GeometryProxy.MarkWarpGeometryComponentDirty(InComponentName);
}

bool FDisplayClusterWarpBlend::CalcFrustumContext(class IDisplayClusterViewport* InViewport, const uint32 InContextNum, const FDisplayClusterWarpEye& InEye, FDisplayClusterWarpContext& OutWarpContext)
{
	if (!GeometryContext.Update(InEye.WorldScale))
	{
		// wrong geometry
		return false;
	}

	bool bShouldUseFrustumCache = GeometryContext.GeometryProxy.GeometryType == EDisplayClusterWarpGeometryType::WarpMap;
	if (bShouldUseFrustumCache)
	{
		FrustumCache.SetFrustumCacheDepth((int32)CVarMPCDIFrustumCacheDepth.GetValueOnAnyThread());
		FrustumCache.SetFrustumCachePrecision((float)CVarMPCDIFrustumCachePrecision.GetValueOnAnyThread());

		if (FrustumCache.GetCachedFrustum(InEye, OutWarpContext))
		{
			return true;
		}
	}

	FDisplayClusterWarpBlendMath_Frustum Frustum(InEye, GeometryContext);

	// Configure logic:
	Frustum.SetFindBestProjectionType(CVarMPCDIProjectionAuto.GetValueOnAnyThread() != 0);
	Frustum.SetParameter((EDisplayClusterWarpBlendFrustumType)(CVarMPCDIFrustumMethod.GetValueOnAnyThread()));
	Frustum.SetParameter((EDisplayClusterWarpBlendStereoMode)(CVarMPCDIStereoMode.GetValueOnAnyThread()));
	Frustum.SetParameter((EDisplayClusterWarpBlendProjectionType)(CVarMPCDIProjectionMode.GetValueOnAnyThread()));

	if (!Frustum.CalcFrustum(InViewport, InContextNum, OutWarpContext))
	{
		return false;
	}

	if (bShouldUseFrustumCache)
	{
		FrustumCache.AddFrustum(InEye, OutWarpContext);
	}

	return true;
}

bool FDisplayClusterWarpBlend::ExportWarpMapGeometry(FMPCDIGeometryExportData* OutMeshData, uint32 InMaxDimension) const
{
#if WITH_EDITOR
	if (GeometryContext.GeometryProxy.WarpMapTexture.IsValid())
	{
		return OutMeshData ? FDisplayClusterWarpBlendExporter_WarpMap::ExportWarpMap(GeometryContext.GeometryProxy.WarpMapTexture.Get(), *OutMeshData, InMaxDimension) : false;
	}
#endif
	return false;
}


