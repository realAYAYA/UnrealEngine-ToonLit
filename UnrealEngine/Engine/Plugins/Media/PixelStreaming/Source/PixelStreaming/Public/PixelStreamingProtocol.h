// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Containers/Array.h"
#include "Containers/Map.h"

namespace Protocol
{
	enum class PIXELSTREAMING_API EPixelStreamingMessageTypes
	{
		Uint8,
		Uint16,
		Int16,
		Float,
		Double,
		Variable,
		Undefined
	};

	enum class PIXELSTREAMING_API EPixelStreamingMessageDirection : uint8
	{
		ToStreamer = 0,
		FromStreamer = 1
	};

	class PIXELSTREAMING_API FPixelStreamingInputMessage
	{
	private:
		static inline uint8 CurrentId = 0;
	public:
		FPixelStreamingInputMessage()
		: Id(++CurrentId)
		, Structure({ EPixelStreamingMessageTypes::Undefined })
		{
		}

		FPixelStreamingInputMessage(uint8 InByteLength, TArray<EPixelStreamingMessageTypes> InStructure)
		: Id(++CurrentId)
		, ByteLength(InByteLength)
		, Structure(InStructure)
		{
		}

		FPixelStreamingInputMessage(uint8 InId)
		: Id(InId)
		, Structure({ EPixelStreamingMessageTypes::Undefined })
		{
			// We set id to 255 for the Protocol message type. 
			// We don't want to set the CurrentId to this value otherwise we will have no room for custom messages
			if(InId > CurrentId && InId != 255)
			{
				CurrentId = InId; // Ensure that any new message id starts after the highest specified Id
			}
		}

		FPixelStreamingInputMessage(uint8 InId, uint8 InByteLength, TArray<EPixelStreamingMessageTypes> InStructure)
		: Id(InId)
		, ByteLength(InByteLength)
		, Structure(InStructure)
		{
			// We set id to 255 for the Protocol message type. 
			// We don't want to set the CurrentId to this value otherwise we will have no room for custom messages
			if(InId > CurrentId && InId != 255)
			{
				CurrentId = InId; // Ensure that any new message id starts after the highest specified Id
			}
		}

		uint8 Id;
		uint8 ByteLength;
		TArray<EPixelStreamingMessageTypes> Structure;
	};

	struct PIXELSTREAMING_API FPixelStreamingProtocol
	{
	public:
		TMap<FString, FPixelStreamingInputMessage> ToStreamerProtocol;
		TMap<FString, FPixelStreamingInputMessage> FromStreamerProtocol;
	};
}; // namespace Protocol
