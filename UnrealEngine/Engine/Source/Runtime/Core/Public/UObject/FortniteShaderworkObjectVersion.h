// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "DevObjectVersion.h"
#include "Containers/Map.h"

// System Guids for changes made in the Fortnite Shaderwork stream
struct FFortniteShaderworkObjectVersion
{
	static CORE_API TMap<FGuid, FGuid> GetSystemGuids();

private:
	FFortniteShaderworkObjectVersion() {}
};
