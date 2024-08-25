// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Types of DC viewport resources
 */
enum class EDisplayClusterViewportResourceType : uint8
{
	// Undefined resource type
	Unknown = 0,

	// This RTT is used to render the scene for the viewport eye context to the specified area on this texture.
	// The mGPU rendering with cross-GPU transfer can be used for this texture.
	InternalRenderTargetResource, /** Just for Internal use */


	/**
	 * Internal textures of the DC rendering pipeline.
	 */

	// Unique Viewport contexts shader resource. No regions used at this point
	// The image source for this resource can be: 'InternalRenderTargetResource', 'OverrideTexture', 'OverrideViewport', etc.
	// This is the entry point to the DC rendering pipeline
	InputShaderResource,

	// Special resource with mips texture, generated from 'InputShaderResource' after postprocess
	MipsShaderResource,

	// Additional targetable resource, used by logic (external warpblend, blur, etc)
	// This resource is used as an additional RTT for 'InputShaderResource'.
	// It contains different contents depending on the point in time: 'PostprocessRTT','ViewportOutputRemap','AfterWarpBlend' etc.
	AdditionalTargetableResource,


	/**
	 * Textures for warp and blend
	 */
	 
	 // The viewport texture before warpblend (InputShaderResource)
	// This resource is used as the source image for the projection policy.
	BeforeWarpBlendTargetableResource,

	// The viewport texture after warpblend (AdditionalTargetableResource or InputShaderResource).
	// This resource is used as the output image in the projection policy.
	AfterWarpBlendTargetableResource,


	/**
	 * Output textures
	 */

	// This is the entry point into the output texture rendering.('OutputFrameTargetableResource' or 'OutputPreviewTargetableResource')
	// The rendering time of this image in the DC rendering pipeline is right after warp&blend.
	OutputTargetableResource,


	/**
	 * Output textures for preview rendering.
	 */

	// If DCRA uses a preview, this texture will be used instead of 'Frame' textures
	OutputPreviewTargetableResource,


	/**
	 * Output 'Frame' textures for cluster rendering
	 * The 'Frame' resources are used to compose all the viewports into a single texture.
	 * There is a separate 'frame' texture for each eye context.
	 * And at the end of the frame these resources are copied to the backbuffer.
	 * 
	 * Projection policy render output to this resources into viewport region
	 * (Context frame region saved FDisplayClusterViewport_Context::FrameTargetRect)
	 */

	// This texture contains the results of the DC rendering, and is copied directly to the backbuffer texture at the end of the frame.
	// Each eye is in a separate texture.
	OutputFrameTargetableResource,

	// This resource is used as an additional RTT for 'OutputFrameTargetableResource'.
	// It contains different contents depending on the point in time: 'FramePostprocessRTT','OutputRemap', etc.
	AdditionalFrameTargetableResource,
};

/**
 * Viewport capture mode
 * This mode affects many viewport rendering settings.
 */
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

/**
 * Viewport can be overridden by another one.
 * This mode determines how many resources will be overridden.
 */
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

/**
* Type of unit for frustum
*/
enum class EDisplayClusterViewport_FrustumUnit: uint8
{
	// 1 unit = 1 pixel
	Pixels = 0,

	// 1 unit = 1 per cent
	Percent
};

/**
 * Special flags for nDisplay Viewport context
 */
enum class EDisplayClusterViewportContextState : uint8
{
	None = 0,

	// The FDisplayClusterViewport::CalculateView() function can be called several times per frame.
	// Each time it must return the same values. For optimization purposes, after the first call this function
	// stores the result in the context variables 'ViewLocation' and 'ViewRotation'.
	// Finally, raises this flag for subsequent calls in the current frame.
	HasCalculatedViewPoint = 1 << 0,

	// Viewpoint is not valid for this viewport (cannot be calculated)
	InvalidViewPoint = 1 << 1,

	// The FDisplayClusterViewport::GetProjectionMatrix() function can also be called several times per frame.
	// stores the result in the context variables 'ProjectionMatrix' and 'OverscanProjectionMatrix'.
	// Finally, raises this flag for subsequent calls in the current frame.
	HasCalculatedProjectionMatrix = 1 << 2,
	HasCalculatedOverscanProjectionMatrix = 1 << 3,

	// The projection matrix is not valid (cannot be calculated)
	InvalidProjectionMatrix = 1 << 4,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportContextState);

/**
* Type of DCRA used by configuration
*/
enum class EDisplayClusterRootActorType : uint8
{
	// This DCRA will be used to render previews. The meshes and preview materials are created at runtime.
	Preview = 1 << 0,

	// A reference to DCRA in the scene, used as a source for math calculations and references.
	// Locations in the scene and math data are taken from this DCRA.
	Scene = 1 << 1,

	// Reference to DCRA, used as a source of configuration data from DCRA and its components.
	Configuration = 1 << 2,

	// This value can only be used in very specific cases:
	// For function GetRootActor() : Return any of the DCRAs that are not nullptr, in ascending order of type: Preview, Scene, Configuration.
	// For function SetRootActor() : Sets all references to DRCA to the specified value.
	Any = Preview | Scene | Configuration,
};
ENUM_CLASS_FLAGS(EDisplayClusterRootActorType);

/**
 * The type of media usage for the viewport.
 */
enum class EDisplayClusterViewportMediaState : uint8
{
	// This viewport does not use media.
	None = 0,

	// This viewport will be captured by the media device.
	Capture = 1 << 0,
	Capture_ForceLateOCIOPass = 1 << 1,

	// This viewport is overridden by the media device.
	Input = 1 << 4,
	Input_ForceLateOCIOPass = 1 << 5,

	ForceLateOCIOPass = Capture_ForceLateOCIOPass | Input_ForceLateOCIOPass,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportMediaState);

