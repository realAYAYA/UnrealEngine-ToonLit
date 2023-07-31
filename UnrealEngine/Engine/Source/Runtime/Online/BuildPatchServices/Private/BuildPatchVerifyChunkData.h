// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FBuildVerifyChunkData
{
public:
	static bool VerifyChunkData(const FString& SearchPath, const FString& OutputFile);
};
