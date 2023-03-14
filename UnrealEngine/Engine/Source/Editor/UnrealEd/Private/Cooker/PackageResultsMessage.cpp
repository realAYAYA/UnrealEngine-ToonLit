// Copyright Epic Games, Inc. All Rights Reserved.

#include "PackageResultsMessage.h"

#include "CookPackageData.h"

namespace UE::Cook
{

void FPackageRemoteResult::AddMessage(const FPackageData& PackageData, const ITargetPlatform* TargetPlatform, const IPackageMessage& Message)
{
	int32 PlatformIndex = Platforms.IndexOfByPredicate([TargetPlatform](const FPlatformResult& Result)
		{ return Result.Platform == TargetPlatform; }
	);
	FPlatformResult* Result;
	if (PlatformIndex != INDEX_NONE)
	{
		Result = &Platforms[PlatformIndex];
	}
	else
	{
		Result = &Platforms.Emplace_GetRef();
		Result->Platform = TargetPlatform;
	}

	FCbWriter Writer;
	Writer.BeginObject();
	Message.Write(Writer, PackageData, TargetPlatform);
	Writer.EndObject();

	Result->Messages.Add(UE::CompactBinaryTCP::FMarshalledMessage{ Message.GetMessageType(), Writer.Save().AsObject() });
}


void FPackageResultsMessage::Write(FCbWriter& Writer) const
{
	Writer.BeginArray("R");
	for (const FPackageRemoteResult& Result : Results)
	{
		Writer.BeginObject();
		{
			Writer << "N" << Result.PackageName;
			Writer << "R" << (uint8) Result.SuppressCookReason;
			Writer << "E" << Result.bReferencedOnlyByEditorOnlyData;
			Writer.BeginArray("P");
			for (const FPackageRemoteResult::FPlatformResult& PlatformResult : Result.Platforms)
			{
				Writer.BeginObject();
				Writer << "S" << PlatformResult.bSuccessful;
				Writer << "G" << PlatformResult.PackageGuid;
				Writer << "D" << PlatformResult.TargetDomainDependencies;
				if (!PlatformResult.Messages.IsEmpty())
				{
					// We write a nonhomogenous array of length 2N. 2N+0 is the Message type, 2N+1 is the message object.
					Writer.BeginArray("M");
					for (const UE::CompactBinaryTCP::FMarshalledMessage& Message : PlatformResult.Messages)
					{
						Writer << Message.MessageType;
						Writer << Message.Object;
					}
					Writer.EndArray();
				}
				Writer.EndObject();
			}
			Writer.EndArray();
		}
		Writer.EndObject();
	}
	Writer.EndArray();
}

bool FPackageResultsMessage::TryRead(FCbObject&& Object)
{
	Results.Reset();
	for (FCbField ResultField : Object["R"])
	{
		FCbObject ResultObject = ResultField.AsObject();
		FPackageRemoteResult& Result = Results.Emplace_GetRef();
		LoadFromCompactBinary(ResultObject["N"], Result.PackageName);
		if (Result.PackageName.IsNone())
		{
			return false;
		}
		int32 LocalSuppressCookReason = ResultObject["R"].AsUInt8(MAX_uint8);
		if (LocalSuppressCookReason == MAX_uint8)
		{
			return false;
		}
		Result.SuppressCookReason = static_cast<ESuppressCookReason>(LocalSuppressCookReason);
		Result.bReferencedOnlyByEditorOnlyData = ResultObject["E"].AsBool();
		Result.Platforms.Reset();
		for (FCbField PlatformField : ResultObject["P"])
		{
			FPackageRemoteResult::FPlatformResult& PlatformResult = Result.Platforms.Emplace_GetRef();
			FCbObject PlatformObject = PlatformField.AsObject();

			PlatformResult.bSuccessful = PlatformObject["S"].AsBool();
			PlatformResult.PackageGuid = PlatformObject["G"].AsUuid();
			PlatformResult.TargetDomainDependencies = PlatformObject["D"].AsObject();

			// We read a nonhomogenous array of length 2N. 2N+0 is the Message type, 2N+1 is the message object.
			FCbArray MessagesArray = PlatformObject["M"].AsArray();
			PlatformResult.Messages.Reserve(MessagesArray.Num() / 2);
			FCbFieldIterator MessageField = MessagesArray.CreateIterator();
			while (MessageField)
			{
				UE::CompactBinaryTCP::FMarshalledMessage& Message = PlatformResult.Messages.Emplace_GetRef();
				Message.MessageType = MessageField->AsUuid();
				if (MessageField.HasError())
				{
					return false;
				}
				++MessageField;
				Message.Object = MessageField->AsObject();
				if (MessageField.HasError())
				{
					return false;
				}
				++MessageField;
			}
		}
	}
	return true;
}

FGuid FPackageResultsMessage::MessageType(TEXT("4631C6C0F6DC4CEFB2B09D3FB0B524DB"));

}