// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Texture/InterchangeTexturePayloadData.h"

namespace UE::Interchange
{
	struct INTERCHANGEIMPORT_API FImportLightProfile : public FImportImage
	{
		float Brightness = 0.0f;

		float TextureMultiplier = 0.0f;
	};
}