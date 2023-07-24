// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "ShaderParameters/DisplayClusterShaderParameters_ICVFX.h"

//***************************************************
//* Runtime configuration from DCRA
//***************************************************
enum class EDisplayClusterViewportICVFXFlags : uint8
{
	None = 0,

	// Allow to use ICVFX for this viewport (Must be supported by projection policy)
	Enable = 1 << 0,

	// Disable incamera render to this viewport
	DisableCamera = 1 << 1,

	// Disable chromakey render to this viewport
	DisableChromakey = 1 << 2,

	// Disable chromakey markers render to this viewport
	DisableChromakeyMarkers = 1 << 3,

	// Disable lightcard render to this viewport
	DisableLightcard = 1 << 4,

	// Use unique lightcard mode for this viewport from param 'OverrideLightcardMode'
	OverrideLightcardMode = 1 << 5,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportICVFXFlags);

//***************************************************
//* This flag raised only from icvfx manager
//***************************************************
enum class EDisplayClusterViewportRuntimeICVFXFlags: uint8
{
	None = 0,

	// Enable use icvfx only from projection policy for this viewport.
	Target = 1 << 0,

	// viewport ICVFX usage
	InCamera    = 1 << 1,
	Chromakey   = 1 << 2,
	Lightcard   = 1 << 3,
	UVLightcard = 1 << 4,

	// This viewport used as internal icvfx composing resource (created and deleted inside icvfx logic)
	InternalResource = 1 << 5,

	// Mark unused icvfx dynamic viewports
	Unused = 1 << 6,
};
ENUM_CLASS_FLAGS(EDisplayClusterViewportRuntimeICVFXFlags);

class FDisplayClusterViewport_RenderSettingsICVFX
{
public:
	// ICVFX logic flags
	EDisplayClusterViewportICVFXFlags         Flags = EDisplayClusterViewportICVFXFlags::None;

	// Internal logic ICVFX flags
	EDisplayClusterViewportRuntimeICVFXFlags  RuntimeFlags = EDisplayClusterViewportRuntimeICVFXFlags::None;

	// Compact ICVFX viewport settings
	FDisplayClusterShaderParameters_ICVFX ICVFX;

public:
	inline void BeginUpdateSettings()
	{
		Flags = EDisplayClusterViewportICVFXFlags::None;

		ICVFX.Reset();

		// Reset target flag
		EnumRemoveFlags(RuntimeFlags, EDisplayClusterViewportRuntimeICVFXFlags::Target);
	}

	// Implement copy ref and arrays
	inline void SetParameters(const FDisplayClusterViewport_RenderSettingsICVFX& InParameters)
	{
		Flags = InParameters.Flags;
		RuntimeFlags = InParameters.RuntimeFlags;

		ICVFX.SetParameters(InParameters.ICVFX);
	}
};
