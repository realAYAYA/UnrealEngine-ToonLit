// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class URuntimeVirtualTextureComponent;

namespace RuntimeVirtualTexture
{
	/** Set the transform on a URuntimeVirtualTextureComponent so that it includes the bounds of all associated primitives in the current world. */
	void VIRTUALTEXTURINGEDITOR_API SetBounds(URuntimeVirtualTextureComponent* InComponent);
};
