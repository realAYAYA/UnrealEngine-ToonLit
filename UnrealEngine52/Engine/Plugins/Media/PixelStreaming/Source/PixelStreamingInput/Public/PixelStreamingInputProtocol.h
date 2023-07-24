// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Containers/Map.h"
#include "PixelStreamingInputMessage.h"
#include "PixelStreamingInputProtocolMap.h"
#include "Dom/JsonObject.h"

class FInputProtocolMap;

struct PIXELSTREAMINGINPUT_API FPixelStreamingInputProtocol
{
public:
	static FInputProtocolMap ToStreamerProtocol;
	static FInputProtocolMap FromStreamerProtocol;

	static TSharedPtr<FJsonObject> ToJson(EPixelStreamingMessageDirection Direction);
};