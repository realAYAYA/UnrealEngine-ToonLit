// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "EngineUtils.h"
#include "ScreenRendering.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"

class FDisplayClusterViewport_Context;
class FRDGBuilder;
class FSceneView;
struct FPostProcessMaterialInputs;
struct FScreenPassTexture;

/**
 * Display Device Proxy object interface
 * [rendering thread]
 */
class DISPLAYCLUSTER_API IDisplayClusterDisplayDeviceProxy
{
public:
	virtual ~IDisplayClusterDisplayDeviceProxy() = default;

public:
	/** Is final pass used
	*/
	virtual bool HasFinalPass_RenderThread() const
	{
		return false;
	}

	/** Add render pass at the end of nDisplay pipeline.
	 *
	 * @param GraphBuilder      - RDG interface.
	 * @param InViewportContext - DC viewport context.
	 * @param InputTextureRHI   - Source texture.
	 * @param InputRect         - Source rec.
	 * @param OutputTextureRHI  - Destination texture.
	 * @param OutputRect        - Destination rec.
	 *
	 * @return - true if success.
	 */
	virtual bool AddFinalPass_RenderThread(FRDGBuilder& GraphBuilder, const FDisplayClusterViewport_Context& InViewportContext,
		FRHITexture2D* InputTextureRHI, const FIntRect& InputRect, FRHITexture2D* OutputTextureRHI, const FIntRect& OutputRect) const
	{ return false; }

	/** Allow callback OnPostProcessPassAfterFXAA.  */
	virtual bool ShouldUsePostProcessPassAfterFXAA() const
	{ return false; }

	/** Callback OnPostProcessPassAfterFXAA.
	 *
	 * @param GraphBuilder - RDG interface
	 * @param View         - Scene View
	 * @param Inputs       - PP Input resources
	 * @param ContextNum   - viewport context index
	 *
	 * @return - Screen pass texture
	 */
	virtual FScreenPassTexture OnPostProcessPassAfterFXAA_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum)
	{ return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder); }

	/** Allow callback OnPostProcessPassAfterSSRInput. */
	virtual bool ShouldUsePostProcessPassAfterSSRInput() const
	{ return false; }

	/** Callback OnPostProcessPassAfterSSRInput.
	 *
	 * @param GraphBuilder - RDG interface
	 * @param View         - Scene View
	 * @param Inputs       - PP Input resources
	 * @param ContextNum   - viewport context index
	 *
	 * @return - Screen pass texture
	 */
	virtual FScreenPassTexture OnPostProcessPassAfterSSRInput_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum)
	{ return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder); }

	/** Allow callback OnPostProcessPassAfterTonemap. */
	virtual bool ShouldUsePostProcessPassTonemap() const
	{ return false; }

	/** Callback OnPostProcessPassAfterTonemap.
	 *
	 * @param GraphBuilder - RDG interface
	 * @param View         - Scene View
	 * @param Inputs       - PP Input resources
	 * @param ContextNum   - viewport context index
	 *
	 * @return - Screen pass texture
	 */
	virtual FScreenPassTexture OnPostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs, const uint32 ContextNum)
	{ return Inputs.ReturnUntouchedSceneColorForPostProcessing(GraphBuilder); }
};
