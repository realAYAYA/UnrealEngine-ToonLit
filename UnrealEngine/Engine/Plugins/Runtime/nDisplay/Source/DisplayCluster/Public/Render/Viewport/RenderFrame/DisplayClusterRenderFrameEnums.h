// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
* Rendering Mode.
*/
enum class EDisplayClusterRenderFrameMode: uint8
{
	Unknown = 0,

	// Render to single backbuffer texture
	Mono,

	// Render to two separated backbuffer textures
	Stereo,

	// Special render: render to two separated backbuffer textures, with half size X
	SideBySide,

	// Special render: render to two separated backbuffer textures, with half size Y
	TopBottom,


	// Monoscopic rendering in PIE
	PIE_Mono,

	// Stereoscopic side-by-side rendering in PIE
	PIE_SideBySide,

	// Stereoscopic top-bottom rendering in PIE
	PIE_TopBottom,

	// Monoscopic rendering for MRQ
	MRQ_Mono,


	// Special render for preview in scene
	PreviewInScene,

	// Special render of ProxyHit for preview in scene
	// *** Not implemented.
	PreviewProxyHitInScene,
};
