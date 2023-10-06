// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BuildPatchSettings.h"

/**
 * A class that controls the process of generating manifests and chunk data from a build image.
 */
class FBuildDataGenerator
{
public:
	/**
	 * Processes a Build directory to create chunks for new data and produce a manifest, saved to the provided cloud directory.
	 * NOTE: This function is blocking and will not return until finished.
	 * @param Configuration         Specifies the settings for the operation. See BuildPatchServices::FChunkBuildConfiguration comments.
	 * @return true if successful.
	 */
	static bool ChunkBuildDirectory(const BuildPatchServices::FChunkBuildConfiguration& Configuration);
};
