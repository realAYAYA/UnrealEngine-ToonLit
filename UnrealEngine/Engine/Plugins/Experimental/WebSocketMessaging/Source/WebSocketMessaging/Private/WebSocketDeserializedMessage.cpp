// Copyright Epic Games, Inc. All Rights Reserved.


#include "WebSocketDeserializedMessage.h"
#include "JsonObjectConverter.h"
#include "WebSocketMessaging.h"

FWebSocketDeserializedMessage::FWebSocketDeserializedMessage()
	: Expiration(FDateTime::MaxValue())	// Make sure messages don't expire if no expiration is specified.
	, Message(nullptr)
	, Scope(EMessageScope::All)
{
}

FWebSocketDeserializedMessage::~FWebSocketDeserializedMessage()
{
	if (Message)
	{
		FMemory::Free(Message);
	}
}

bool FWebSocketDeserializedMessage::ParseJson(const FString& Json)
{
	static TMap<FString, EMessageScope> MessageScopeStringMapping =
	{
		{"Thread", EMessageScope::Thread},
		{ "Process", EMessageScope::Process },
		{ "Network", EMessageScope::Network },
		{ "All", EMessageScope::All }
	};

	TSharedPtr<FJsonValue> RootValue;

	TSharedRef<TJsonReader<TCHAR>> JsonReader = TJsonReaderFactory<TCHAR>::Create(Json);
	if (FJsonSerializer::Deserialize(JsonReader, RootValue))
	{
		TSharedPtr<FJsonObject> RootObject = RootValue->AsObject();
		if (!RootObject)
		{
			return false;
		}

		FString MessageType;
		if (!RootObject->TryGetStringField(WebSocketMessaging::Tag::MessageType, MessageType))
		{
			return false;
		}

		FString JsonSender;
		if (!RootObject->TryGetStringField(WebSocketMessaging::Tag::Sender, JsonSender))
		{
			return false;
		}

		if (!FMessageAddress::Parse(JsonSender, Sender))
		{
			return false;
		}

		const TSharedPtr<FJsonObject>* JsonAnnotations = nullptr;
		if (RootObject->TryGetObjectField(WebSocketMessaging::Tag::Annotations, JsonAnnotations))
		{
			for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*JsonAnnotations)->Values)
			{
				Annotations.Add(*Pair.Key, Pair.Value->AsString());
			}
		}

		const TSharedPtr<FJsonObject>* JsonMessage = nullptr;
		if (!RootObject->TryGetObjectField(WebSocketMessaging::Tag::Message, JsonMessage))
		{
			return false;
		}

		UScriptStruct* ScriptStruct = FindObjectSafe<UScriptStruct>(nullptr, *MessageType);
		if (!ScriptStruct)
		{
			return false;
		}

		TypeInfo = ScriptStruct;

		if (Message)
		{
			FMemory::Free(Message);
		}

		Message = FMemory::Malloc(ScriptStruct->GetStructureSize());
		ScriptStruct->InitializeStruct(Message);

		if (!FJsonObjectConverter::JsonObjectToUStruct(JsonMessage->ToSharedRef(), ScriptStruct, Message))
		{
			FMemory::Free(Message);
			Message = nullptr;
			return false;
		}

		int64 UnixTime;
		if (RootObject->TryGetNumberField(WebSocketMessaging::Tag::Expiration, UnixTime))
		{
			Expiration = FDateTime::FromUnixTimestamp(UnixTime);
		}

		if (RootObject->TryGetNumberField(WebSocketMessaging::Tag::TimeSent, UnixTime))
		{
			TimeSent = FDateTime::FromUnixTimestamp(UnixTime);
		}

		FString ScopeString;
		if (RootObject->TryGetStringField(WebSocketMessaging::Tag::Scope, ScopeString))
		{
			if (MessageScopeStringMapping.Contains(ScopeString))
			{
				Scope = MessageScopeStringMapping[ScopeString];
			}
			// unknown scope string
			else
			{
				return false;
			}
		}

		TArray<FString> RecipientsStrings;
		if (RootObject->TryGetStringArrayField(WebSocketMessaging::Tag::Recipients, RecipientsStrings))
		{
			for (const FString& RecipientString : RecipientsStrings)
			{
				FMessageAddress RecipientAddress;
				if (FMessageAddress::Parse(RecipientString, RecipientAddress))
				{
					Recipients.Add(RecipientAddress);
				}
			}
		}

		return true;
	}

	return false;
}
