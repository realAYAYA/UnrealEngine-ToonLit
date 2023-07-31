// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EDisplayClusterRenderFrameMode: uint8
{
	// Render to single backbuffer texture
	Mono,

	// Render to two separated backbuffer textures
	Stereo,

	// Special render: render to two separated backbuffer textures, with half size X
	SideBySide,

	// Special render: render to two separated backbuffer textures, with half size Y
	TopBottom,

	// Special render for preview in scene
	PreviewInScene,
};

// (experimental, not implemented, reserved)
enum class EDisplayClusterRenderFamilyMode : uint8
{
	// Render all viewports to unique RenderTargets
	None = 0,

	// Merge views by ViewFamilyGroupNum
	AllowMergeForGroups,

	// Merge views by ViewFamilyGroupNum and stereo
	AllowMergeForGroupsAndStereo,

	// Use rules to merge views to minimal num of families (separate by: buffer_ratio, viewExtension, max RTT size)
	MergeAnyPossible,
};


// Control multiGPU for cluster
enum class EDisplayClusterMultiGPUMode : uint8
{
	// Disable multi GPU rendering
	None = 0,

	// Use default crossGPU transfer
	Enabled,

	// Performance (Experimental): Use optimized transfer once per frame with bLockStepGPUs=true
	Optimized_EnabledLockSteps,

	// Performance (Experimental): Use optimized transfer once per frame with bLockStepGPUs=false 
	Optimized_DisabledLockSteps,
};
