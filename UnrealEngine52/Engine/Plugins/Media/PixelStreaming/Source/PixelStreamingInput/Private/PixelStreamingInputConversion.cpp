// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingInputConversion.h"

TMap<TTuple<EPixelStreamingXRSystem, EControllerHand, uint8, EPixelStreamingInputAction>, FKey> FPixelStreamingInputConverter::XRInputToFKey;
TMap<TTuple<uint8, EPixelStreamingInputAction>, FKey> FPixelStreamingInputConverter::GamepadInputToFKey;
