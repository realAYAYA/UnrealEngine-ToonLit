// Copyright Epic Games, Inc. All Rights Reserved.

#include "PixelStreamingInputProtocol.h"
#include "PixelStreamingInputMessage.h"
#include "Dom/JsonValue.h"

FInputProtocolMap FPixelStreamingInputProtocol::ToStreamerProtocol;
FInputProtocolMap FPixelStreamingInputProtocol::FromStreamerProtocol;

TSharedPtr<FJsonObject> FPixelStreamingInputProtocol::ToJson(EPixelStreamingMessageDirection Direction)
{
	TSharedPtr<FJsonObject> ProtocolJson = MakeShareable(new FJsonObject());
	FInputProtocolMap MessageProtocol =
		(Direction == EPixelStreamingMessageDirection::ToStreamer)
		? FPixelStreamingInputProtocol::ToStreamerProtocol
		: FPixelStreamingInputProtocol::FromStreamerProtocol;

	ProtocolJson->SetField("Direction", MakeShared<FJsonValueNumber>(static_cast<uint8>(Direction)));
	MessageProtocol.Apply([ProtocolJson](FString Key, FPixelStreamingInputMessage Value) {
		TSharedPtr<FJsonObject> MessageJson = MakeShareable(new FJsonObject());

		MessageJson->SetField("id", MakeShared<FJsonValueNumber>(Value.GetID()));
		MessageJson->SetField("byteLength", MakeShared<FJsonValueNumber>(Value.GetByteLength()));

		if (Value.GetByteLength() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> StructureJson;
			TArray<EPixelStreamingMessageTypes> Structure = Value.GetStructure();
			for (auto It = Structure.CreateIterator(); It; ++It)
			{
				FString Text;
				switch (*It)
				{
					case EPixelStreamingMessageTypes::Uint8:
						Text = "uint8";
						break;
					case EPixelStreamingMessageTypes::Uint16:
						Text = "uint16";
						break;
					case EPixelStreamingMessageTypes::Int16:
						Text = "int16";
						break;
					case EPixelStreamingMessageTypes::Float:
						Text = "float";
						break;
					case EPixelStreamingMessageTypes::Double:
						Text = "double";
						break;
					default:
						Text = "";
				}
				TSharedRef<FJsonValueString> JsonValue = MakeShareable(new FJsonValueString(*Text));
				StructureJson.Add(JsonValue);
			}
			MessageJson->SetArrayField("structure", StructureJson);
		}

		ProtocolJson->SetField(*Key, MakeShared<FJsonValueObject>(MessageJson));
	});

	return ProtocolJson;
}