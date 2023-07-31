// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DisplayClusterWarpEnums.h"
#include "DisplayClusterWarpContext.h"

class IDisplayClusterRenderTexture;
class UStaticMeshComponent;
class USceneComponent;
struct FMPCDIGeometryExportData;

class IDisplayClusterWarpBlend
{
public:
	virtual ~IDisplayClusterWarpBlend() = default;

public:
	/**
	* Calculate warp context data for new eye
	*
	* @param InEye           - Current eye and scene
	* @param OutWarpContext  - Output context
	*
	* @return - true if the context calculated successfully
	*/
	virtual bool CalcFrustumContext(class IDisplayClusterViewport* InViewport, const uint32 InContextNum, const FDisplayClusterWarpEye& InEye, FDisplayClusterWarpContext& OutWarpContext) = 0;

	// Access to resources
	virtual class FRHITexture* GetTexture(EDisplayClusterWarpBlendTextureType TextureType) const = 0;
	virtual float              GetAlphaMapEmbeddedGamma() const = 0;

	virtual const class IDisplayClusterRender_MeshComponentProxy* GetWarpMeshProxy_RenderThread() const = 0;

	// Return current warp profile type
	virtual EDisplayClusterWarpProfileType  GetWarpProfileType() const = 0;

	// Return current warp resource type
	virtual EDisplayClusterWarpGeometryType GetWarpGeometryType() const = 0;

	virtual bool ExportWarpMapGeometry(FMPCDIGeometryExportData* OutMeshData, uint32 InMaxDimension = 0) const = 0;

	/**
	* Mark internal component ref as dirty for geometry update
	*
	* @param InComponentName - (optional) the name of the internal geometry ref component. Empty string for any component name
	*
	* @return - true, if there is a marked.
	*/
	virtual bool MarkWarpGeometryComponentDirty(const FName& InComponentName) = 0;
};
