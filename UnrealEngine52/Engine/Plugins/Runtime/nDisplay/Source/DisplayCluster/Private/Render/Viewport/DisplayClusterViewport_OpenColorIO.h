// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "OpenColorIOColorSpace.h"
#include "OpenColorIORendering.h"

#include "RHI.h"
#include "RHICommandList.h"
#include "RHIResources.h"
#include "Templates/SharedPointer.h"

class FSceneView;
class FSceneViewFamily;
struct FScreenPassTexture;
struct FPostProcessMaterialInputs;
class FRDGBuilder;
class FDisplayClusterViewport_Context;

/**
 * nDisplay OCIO implementation.
 * 
 */
class FDisplayClusterViewport_OpenColorIO
	: public TSharedFromThis<FDisplayClusterViewport_OpenColorIO, ESPMode::ThreadSafe>
{
public:
	FDisplayClusterViewport_OpenColorIO(const FOpenColorIOColorConversionSettings& InDisplayConfiguration);
	virtual ~FDisplayClusterViewport_OpenColorIO();

public:
	/** Update render thread resources. */
	void UpdateOpenColorIORenderPassResources();

	/** Setup view for OCIO.
	 *
	 * @param InOutViewFamily - [In,Out] ViewFamily.
	 * @param InOutView       - [In,Out] View.
	 *
	 * @return - none.
	 */
	void SetupSceneView(FSceneViewFamily& InOutViewFamily, FSceneView& InOutView) const;

	/** Add OCIO render pass.
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
	bool AddPass_RenderThread(FRDGBuilder& GraphBuilder, const FDisplayClusterViewport_Context& InViewportContext,
		FRHITexture2D* InputTextureRHI, const FIntRect& InputRect, FRHITexture2D* OutputTextureRHI, const FIntRect& OutputRect, bool bUnpremultiply, bool InvertedAlpha) const;

	/* This is a copy of FOpenColorIODisplayExtension::PostProcessPassAfterTonemap_RenderThread() */
	FScreenPassTexture PostProcessPassAfterTonemap_RenderThread(FRDGBuilder& GraphBuilder, const FDisplayClusterViewport_Context& InViewportContext, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs);

	/** Is OCIO enabled for render thread */
	bool IsEnabled_RenderThread() const;

	/** Compare two OCIO configurations.
	 *
	 * @param InConversionSettings - configuration to compare with.
	 *
	 * @return - true if equal.
	 */
	bool IsConversionSettingsEqual(const FOpenColorIOColorConversionSettings& InConversionSettings) const;

	/** Get current OCIO conversion settings. */
	const FOpenColorIOColorConversionSettings& GetConversionSettings() const
	{
		return ConversionSettings;
	}

private:
	/* returns the DisplayGamma of OCIO for the viewport context.*/
	float GetDisplayGamma(const FDisplayClusterViewport_Context& InViewportContext) const;

private:
	/** Cached pass resources required to apply conversion for render thread. */
	FOpenColorIORenderPassResources CachedResourcesRenderThread;

	/** Configuration to apply during post render callback. */
	FOpenColorIOColorConversionSettings ConversionSettings;

	/** Shader resources state. */
	bool bShaderResourceValid = true;

	/** Configuration data state. */
	TAtomic<bool> bIsEnabled = true;
};
