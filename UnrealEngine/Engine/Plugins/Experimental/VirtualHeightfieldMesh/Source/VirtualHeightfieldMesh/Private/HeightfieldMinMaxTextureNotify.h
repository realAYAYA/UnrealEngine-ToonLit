// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class UHeightfieldMinMaxTexture;

namespace VirtualHeightfieldMesh
{
#if WITH_EDITOR
	/**
	 * Find any UVirtualHeightfieldMeshComponent that reference this UHeightfieldMinMaxTexture and mark them dirty.
	 * We need to do this after editing the UHeightfieldMinMaxTexture settings.
	 */
	void NotifyComponents(UHeightfieldMinMaxTexture const* MinMaxTexture);
#endif
}
