// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PixelStreamingInputEnums.h"

#include "Containers/Array.h"
#include "HAL/Platform.h"

class PIXELSTREAMINGINPUT_API FPixelStreamingInputMessage
{
public:
	// Emtpy constructor. Sets ID to next available ID, byteLength to 0, and an empty Structure
	FPixelStreamingInputMessage();

	// Constructor taking an ID. Sets byteLength to 0, and an empty Structure
	FPixelStreamingInputMessage(uint8 InId);

	// Constructor taking a Structure. Sets ID to next available ID and calculates bytelength from the structure
	FPixelStreamingInputMessage(TArray<EPixelStreamingMessageTypes> InStructure);

	// Constructor taking an ID and a Structure. Calculates bytelength from the structure
	FPixelStreamingInputMessage(uint8 InId, TArray<EPixelStreamingMessageTypes> InStructure);

	uint8 GetByteLength() const { return ByteLength; }
	uint8 GetID() const { return Id; }
	TArray<EPixelStreamingMessageTypes> GetStructure() const { return Structure; }

private:
	uint8 CalculateByteLength();

	uint8 Id;
	TArray<EPixelStreamingMessageTypes> Structure;
	uint8 ByteLength;

	static uint8 CurrentId;
};