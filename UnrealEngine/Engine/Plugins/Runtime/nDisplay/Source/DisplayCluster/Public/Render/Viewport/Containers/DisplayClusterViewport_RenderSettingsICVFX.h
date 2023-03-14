// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ShaderParameters/DisplayClusterShaderParameters_ICVFX.h"

enum EDisplayClusterViewportICVFXFlags
{
	ViewportICVFX_None = 0,

	// Allow to use ICVFX for this viewport (Must be supported by projection policy)
	ViewportICVFX_Enable = 1 << 0,
	// Disable incamera render to this viewport
	ViewportICVFX_DisableCamera = 1 << 1,
	// Disable chromakey render to this viewport
	ViewportICVFX_DisableChromakey = 1 << 2,
	// Disable chromakey markers render to this viewport
	ViewportICVFX_DisableChromakeyMarkers = 1 << 3,
	// Disable lightcard render to this viewport
	ViewportICVFX_DisableLightcard = 1 << 4,
	// Use unique lightcard mode for this viewport from param 'OverrideLightcardMode'
	ViewportICVFX_OverrideLightcardMode = 1 << 5,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportICVFXFlags);

//***************************************************
//* This flag raised only from icvfx manager
//***************************************************
enum EDisplayClusterViewportRuntimeICVFXFlags
{
	ViewportRuntimeICVFX_None = 0,

	// Enable use icvfx only from projection policy for this viewport.
	ViewportRuntime_ICVFXTarget = 1 << 0,

	// viewport ICVFX usage
	ViewportRuntime_ICVFXIncamera       = 1 << 1,
	ViewportRuntime_ICVFXChromakey      = 1 << 2,
	ViewportRuntime_ICVFXLightcard      = 1 << 3,

	// This viewport used as internal icvfx composing resource (created and deleted inside icvfx logic)
	ViewportRuntime_InternalResource = 1 << 30,
	// Mark unused icvfx dynamic viewports
	ViewportRuntime_Unused = 1 << 31,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportRuntimeICVFXFlags);

class FDisplayClusterViewport_RenderSettingsICVFX
{
public:
	// ICVFX logic flags
	EDisplayClusterViewportICVFXFlags         Flags = ViewportICVFX_None;

	// Internal logic ICVFX flags
	EDisplayClusterViewportRuntimeICVFXFlags  RuntimeFlags = ViewportRuntimeICVFX_None;

	// Compact ICVFX viewport settings
	FDisplayClusterShaderParameters_ICVFX ICVFX;

public:
	inline void BeginUpdateSettings()
	{
		Flags = ViewportICVFX_None;

		ICVFX.Reset();

		// Reset target flag
		RuntimeFlags &= ~(ViewportRuntime_ICVFXTarget);
	}

	// Implement copy ref and arrays
	inline void SetParameters(const FDisplayClusterViewport_RenderSettingsICVFX& InParameters)
	{
		Flags = InParameters.Flags;
		RuntimeFlags = InParameters.RuntimeFlags;

		ICVFX.SetParameters(InParameters.ICVFX);
	}
};
