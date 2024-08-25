// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class URuntimeVirtualTextureComponent;
enum class EShadingPath;

namespace RuntimeVirtualTexture
{
	/** Returns true if the component describes a runtime virtual texture that has streaming mips. */
	bool HasStreamedMips(URuntimeVirtualTextureComponent* InComponent);
	bool HasStreamedMips(EShadingPath ShadingPath, URuntimeVirtualTextureComponent* InComponent);

	/** Build the streaming mips texture. */
	bool BuildStreamedMips(URuntimeVirtualTextureComponent* InComponent, FLinearColor const& FixedColor);
	bool BuildStreamedMips(EShadingPath ShadingPath, URuntimeVirtualTextureComponent* InComponent, FLinearColor const& FixedColor);
};
