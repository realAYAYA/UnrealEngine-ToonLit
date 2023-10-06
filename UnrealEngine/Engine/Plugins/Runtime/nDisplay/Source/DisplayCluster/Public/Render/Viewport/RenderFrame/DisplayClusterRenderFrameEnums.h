// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

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
