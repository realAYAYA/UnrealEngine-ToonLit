// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DevObjectVersion.h"
#include "Containers/Map.h"

// System Guids for changes made in the Fortnite Shaderwork stream
struct CORE_API FFortniteShaderworkObjectVersion
{
	static TMap<FGuid, FGuid> GetSystemGuids();

private:
	FFortniteShaderworkObjectVersion() {}
};