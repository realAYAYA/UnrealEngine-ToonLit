// Copyright Epic Games, Inc. All Rights Reserved.

#include "Json/GLTFJsonLightMap.h"

void FGLTFJsonLightMap::WriteObject(IGLTFJsonWriter& Writer) const
{
	if (!Name.IsEmpty())
	{
		Writer.Write(TEXT("name"), Name);
	}

	if (Texture.Index != nullptr)
	{
		Writer.Write(TEXT("texture"), Texture);
	}

	Writer.Write(TEXT("lightmapScale"), LightMapScale);
	Writer.Write(TEXT("lightmapAdd"), LightMapAdd);
	Writer.Write(TEXT("coordinateScaleBias"), CoordinateScaleBias);
}
