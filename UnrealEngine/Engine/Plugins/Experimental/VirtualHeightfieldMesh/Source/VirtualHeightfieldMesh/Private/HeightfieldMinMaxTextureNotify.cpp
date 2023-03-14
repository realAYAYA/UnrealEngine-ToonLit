// Copyright Epic Games, Inc. All Rights Reserved.

#include "HeightfieldMinMaxTextureNotify.h"

#include "UObject/UObjectIterator.h"
#include "VirtualHeightfieldMeshComponent.h"

namespace VirtualHeightfieldMesh
{
#if WITH_EDITOR

	void NotifyComponents(UHeightfieldMinMaxTexture const* MinMaxTexture)
	{
		for (TObjectIterator<UVirtualHeightfieldMeshComponent> It; It; ++It)
		{
			if (It->GetMinMaxTexture() == MinMaxTexture)
			{
				It->MarkRenderStateDirty();
			}
		}
	}

#endif
}
