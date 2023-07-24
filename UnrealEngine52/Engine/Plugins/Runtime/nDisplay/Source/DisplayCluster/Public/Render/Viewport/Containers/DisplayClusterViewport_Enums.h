// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EDisplayClusterViewportResourceType : uint8
{
	// All viewport contexts rendered to render target (using atlasing to map multiple views)	
	// Then called single cross gpu transfer for multiple render targets regions
	// After corss gpu transfers viewport regions resolved to unique viewports contexts resources 'InputShaderResource'
	// (Context region saved FDisplayClusterViewport_Context::RenderTargetRect)
	// * this resource used in next function for postprocess and mips generation:
	//
	//    FDisplayClusterViewportProxy::UpdateDeferredResources()
	//     {
	//       Resolve(InternalRenderTargetResource -> InputShaderResource)
	//       Postprocess(InputShaderResource -> AdditionalTargetableResource -> InputShaderResource)
	//.......GenerateMips(InputShaderResource -> MipsShaderResource)
	//     }
	//
	InternalRenderTargetResource = 0, /** Just for Internal use */

	// Unique Viewport contexts shader resource. No regions used at this point
	// Use this resources as source for projection policy and resolve to frame targets
	InputShaderResource,

	// Special resource with mips texture, generated from 'InputShaderResource' after postprocess
	MipsShaderResource,

	// Additional targetable resource, used by logic (external warpblend, blur, etc)
	AdditionalTargetableResource,

	// Final frame targetable resources used externally as frame output
	// Projection policy render output to this resources into viewport region
	// (Context frame region saved FDisplayClusterViewport_Context::FrameTargetRect)
	OutputFrameTargetableResource,

	// Some frame postprocess require this additional full frame resource
	AdditionalFrameTargetableResource,

	// Finally OutputFrameTargetableResource resolved from OutputFrameTargetableResource to the Backbuffer from external logic
#if WITH_EDITOR
	OutputPreviewTargetableResource,
#endif
};

enum class EDisplayClusterViewportCaptureMode : uint8
{
	// Use current scene format, no alpha
	Default = 0,

	// use small BGRA 8bit texture with alpha for masking
	Chromakey,

	// use hi-res float texture with alpha for compisiting
	Lightcard,

	// Special hi-res mode for movie pipeline
	MoviePipeline,
};

enum class EDisplayClusterViewportOverrideMode : uint8
{
	// Do not override this viewport from the other one (Render viewport; create all resources)
	None = 0,

	// Override internalRTT from the other viewport (Don't render this viewport; Don't create RTT resource)
	// Useful for custom PP on the same InRTT. (OCIO per-viewport\node)
	InernalRTT,

	// Override all - clone viewport (Dont render; Don't create resources;)
	All
};
