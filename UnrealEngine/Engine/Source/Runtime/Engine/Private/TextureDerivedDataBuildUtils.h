// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_EDITOR

#include "CoreTypes.h"
#include "DerivedDataSharedStringFwd.h"

class FCbObject;
class FName;
class UTexture;
struct FTextureBuildSettings;

UE::DerivedData::FUtf8SharedString FindTextureBuildFunction(FName TextureFormatName);
FCbObject SaveTextureBuildSettings(const UTexture& Texture, const FTextureBuildSettings& BuildSettings, int32 LayerIndex, bool bUseCompositeTexture, int64 RequiredMemoryEstimate);

#endif // WITH_EDITOR
