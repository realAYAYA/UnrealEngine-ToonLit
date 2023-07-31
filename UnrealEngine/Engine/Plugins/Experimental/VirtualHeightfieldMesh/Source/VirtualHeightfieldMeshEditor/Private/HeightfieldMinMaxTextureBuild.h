// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UVirtualHeightfieldMeshComponent;

namespace VirtualHeightfieldMesh
{
	/** Returns true if the component has a MinMax height texture. */
	bool HasMinMaxHeightTexture(UVirtualHeightfieldMeshComponent* InComponent);

	/** Build the MinMax height texture. */
	bool BuildMinMaxHeightTexture(UVirtualHeightfieldMeshComponent* InComponent);
};
