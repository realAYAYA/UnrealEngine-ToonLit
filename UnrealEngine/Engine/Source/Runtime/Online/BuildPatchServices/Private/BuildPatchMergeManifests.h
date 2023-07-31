// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "BuildPatchManifest.h"

class FBuildMergeManifests
{
public:
	static bool MergeManifests(const FString& ManifestFilePathA, const FString& ManifestFilePathB, const FString& ManifestFilePathC, const FString& NewVersionString, const FString& SelectionDetailFilePath);

	static FBuildPatchAppManifestPtr MergeDeltaManifest(const FBuildPatchAppManifestRef& Manifest, const FBuildPatchAppManifestRef& Delta);
};
