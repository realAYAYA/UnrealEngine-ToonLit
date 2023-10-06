// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"

class UTexture;
class UTextureFactory;

namespace UE::NormalMapIdentification
{
	/**
	 * Handle callback when an asset is imported.
	 * @param	InFactory	The texture factory being used.
	 * @param	InTexture	The texture that was imported.
	 * 
	 * @return true if the asset was identified as normal
	 */
	bool TEXTUREUTILITIESCOMMON_API HandleAssetPostImport( UTexture* InTexture );
}

#endif ///WITH_EDITOR
