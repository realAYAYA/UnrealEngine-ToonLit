// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithFBXSceneProcessor.h"

#include "CoreTypes.h"

struct FDatasmithFBXScene;
struct FDeltaGenTmlDataTimeline;
struct FDirectoryPath;

class FDatasmithDeltaGenSceneProcessor : public FDatasmithFBXSceneProcessor
{
public:
	FDatasmithDeltaGenSceneProcessor(FDatasmithFBXScene* InScene);

	/**
	 * Fetches AO textures matching corresponding mesh names, and assigns them to materials used for each node.
	 * May create additional materials, as we may have to clone material instances to use different AO textures per mesh.
	 */
	void SetupAOTextures(const TArray<FDirectoryPath>& TextureFolders);

	/**
	 * Decompose all scene nodes with nonzero rotation and scaling pivots
	 * using dummy actors, and handle their animations
	 */
	void DecomposePivots(TArray<FDeltaGenTmlDataTimeline>& Timelines);
};