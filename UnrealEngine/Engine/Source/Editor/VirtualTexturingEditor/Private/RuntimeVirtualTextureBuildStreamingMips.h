// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ERuntimeVirtualTextureDebugType;
class URuntimeVirtualTextureComponent;

namespace RuntimeVirtualTexture
{
	/** Returns true if the component describes a runtime virtual texture that has streaming mips. */
	bool HasStreamedMips(URuntimeVirtualTextureComponent* InComponent);

	/** Build the streaming mips texture. */
	bool BuildStreamedMips(URuntimeVirtualTextureComponent* InComponent, ERuntimeVirtualTextureDebugType DebugType);
};
