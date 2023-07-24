// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingInputMessage.h"
#include "PixelStreamingInputEnums.h"

uint8 FPixelStreamingInputMessage::CurrentId = 0;

FPixelStreamingInputMessage::FPixelStreamingInputMessage()
	: Id(++CurrentId)
	, Structure({})
{
	ByteLength = 0;
}

FPixelStreamingInputMessage::FPixelStreamingInputMessage(uint8 InId)
	: Id(InId)
	, Structure({})
{
	// We set id to 255 for the Protocol message type.
	// We don't want to set the CurrentId to this value otherwise we will have no room for custom messages
	if (InId > CurrentId && InId != 255)
	{
		CurrentId = InId; // Ensure that any new message id starts after the highest specified Id
	}

	ByteLength = 0;
}

FPixelStreamingInputMessage::FPixelStreamingInputMessage(TArray<EPixelStreamingMessageTypes> InStructure)
	: Id(++CurrentId)
	, Structure(InStructure)
{
	ByteLength = CalculateByteLength();
}

FPixelStreamingInputMessage::FPixelStreamingInputMessage(uint8 InId, TArray<EPixelStreamingMessageTypes> InStructure)
	: Id(InId)
	, Structure(InStructure)
{
	// We set id to 255 for the Protocol message type.
	// We don't want to set the CurrentId to this value otherwise we will have no room for custom messages
	if (InId > CurrentId && InId != 255)
	{
		CurrentId = InId; // Ensure that any new message id starts after the highest specified Id
	}

	ByteLength = CalculateByteLength();
}

uint8 FPixelStreamingInputMessage::CalculateByteLength()
{
	uint8 Length = 0;
	for (EPixelStreamingMessageTypes Type : Structure)
	{
		switch (Type)
		{
			case EPixelStreamingMessageTypes::Uint8:
				Length += 1;
				break;
			case EPixelStreamingMessageTypes::Uint16:
			case EPixelStreamingMessageTypes::Int16:
				Length += 2;
				break;
			case EPixelStreamingMessageTypes::Float:
				Length += 4;
				break;
			case EPixelStreamingMessageTypes::Double:
				Length += 8;
				break;
			case EPixelStreamingMessageTypes::Variable:
			case EPixelStreamingMessageTypes::Undefined:
			default:
				// These types of have no defined length
				break;
		}
	}
	return Length;
}