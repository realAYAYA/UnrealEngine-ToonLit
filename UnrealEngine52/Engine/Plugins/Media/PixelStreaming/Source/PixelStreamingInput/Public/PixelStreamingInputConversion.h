// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "InputCoreTypes.h"
#include "PixelStreamingHMDEnums.h"
#include "PixelStreamingInputEnums.h"
#include "GenericPlatform/GenericApplicationMessageHandler.h"

struct PIXELSTREAMINGINPUT_API FPixelStreamingInputConverter
{
public:
	static TMap<TTuple<EPixelStreamingXRSystem, EControllerHand, uint8, EPixelStreamingInputAction>, FKey> XRInputToFKey;
	static TMap<TTuple<uint8, EPixelStreamingInputAction>, FKey> GamepadInputToFKey;
};