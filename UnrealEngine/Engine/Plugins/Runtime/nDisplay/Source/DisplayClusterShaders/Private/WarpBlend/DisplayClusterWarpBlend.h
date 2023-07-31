// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "WarpBlend/IDisplayClusterWarpBlend.h"
#include "WarpBlend/DisplayClusterWarpBlend_GeometryContext.h"
#include "WarpBlend/DisplayClusterWarpBlend_FrustumCache.h"

#include "Render/IDisplayClusterRenderTexture.h"

#include "WarpBlend/Exporter/DisplayClusterWarpBlendExporter_WarpMap.h"

class FDisplayClusterWarpBlend
	: public IDisplayClusterWarpBlend
{
public:
	FDisplayClusterWarpBlend()
	{}

	//~ IDisplayClusterWarpBlend
	virtual ~FDisplayClusterWarpBlend() override
	{ }

	virtual bool MarkWarpGeometryComponentDirty(const FName& InComponentName) override;

	virtual bool CalcFrustumContext(class IDisplayClusterViewport* InViewport, const uint32 InContextNum, const FDisplayClusterWarpEye& InEye, FDisplayClusterWarpContext& OutWarpContext) override;

	// Access to resources
	virtual class FRHITexture* GetTexture(EDisplayClusterWarpBlendTextureType TextureType) const override
	{
		const IDisplayClusterRenderTexture* pTexture = GeometryContext.GeometryProxy.ImplGetTexture(TextureType);
		return (pTexture != nullptr) ? pTexture->GetRHITexture() : nullptr;
	}

	virtual float              GetAlphaMapEmbeddedGamma() const override
	{ 
		return GeometryContext.GeometryProxy.AlphaMapEmbeddedGamma; 
	}

	virtual const IDisplayClusterRender_MeshComponentProxy* GetWarpMeshProxy_RenderThread() const override
	{
		return GeometryContext.GeometryProxy.GetWarpMeshProxy_RenderThread();
	}

	// Return current warp profile type
	virtual EDisplayClusterWarpProfileType  GetWarpProfileType() const override
	{
		return GeometryContext.ProfileType;
	}

	// return current warp resource type
	virtual EDisplayClusterWarpGeometryType GetWarpGeometryType() const override
	{
		return GeometryContext.GeometryProxy.GeometryType;
	}

	virtual bool ExportWarpMapGeometry(FMPCDIGeometryExportData* OutMeshData, uint32 InMaxDimension = 0) const override final;

	//~!IDisplayClusterWarpBlend

public:
	FDisplayClusterWarpBlend_GeometryContext GeometryContext;

protected:
	// Frustum cache for huge WarpMap
	FDisplayClusterWarpBlend_FrustumCache FrustumCache;
};
