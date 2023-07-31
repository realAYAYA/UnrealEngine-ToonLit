// Copyright Epic Games, Inc. All Rights Reserved.

#include "Cluster/DisplayClusterClusterEvent.h"

#include "Misc/DisplayClusterLog.h"
#include "Misc/DisplayClusterStrings.h"
#include "Misc/DisplayClusterTypesConverter.h"


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterClusterEventJson
//////////////////////////////////////////////////////////////////////////////////////////////
FString FDisplayClusterClusterEventJson::SerializeToString() const
{
	return FString::Printf(TEXT("%s:%s:%s:%d:%d:%s"),
		*Category,
		*Type,
		*Name,
		bIsSystemEvent ? 1 : 0,
		bShouldDiscardOnRepeat ? 1 : 0,
		*SerializeParametersToString());
}

bool FDisplayClusterClusterEventJson::DeserializeFromString(const FString& Arch)
{
	const FString TokenSeparator(TEXT(":"));
	FString TempStr;

	FString StrIsSystemEvent;
	FString StrShouldDiscardOnRepeat;

	if (Arch.Split(TokenSeparator, &Category, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false ||
		TempStr.Split(TokenSeparator, &Type, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false ||
		TempStr.Split(TokenSeparator, &Name, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false ||
		TempStr.Split(TokenSeparator, &StrIsSystemEvent, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false ||
		TempStr.Split(TokenSeparator, &StrShouldDiscardOnRepeat, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false)
	{
		return false;
	}

	bIsSystemEvent = DisplayClusterTypesConverter::template FromString<bool>(StrIsSystemEvent);
	bShouldDiscardOnRepeat = DisplayClusterTypesConverter::template FromString<bool>(StrShouldDiscardOnRepeat);

	if (DeserializeParametersFromString(TempStr) == false)
	{
		return false;
	}

	return true;
}

FString FDisplayClusterClusterEventJson::SerializeParametersToString() const
{
	FString Result;

	for (const auto& obj : Parameters)
	{
		Result += FString::Printf(TEXT("%s%s%s;"), *obj.Key, DisplayClusterStrings::common::KeyValSeparator, *obj.Value);
	}

	return Result;
}

bool FDisplayClusterClusterEventJson::DeserializeParametersFromString(const FString& Arch)
{
	Parameters.Empty(Parameters.Num());

	FString TempStr = Arch;
	FString TempKeyValPair;

	while (TempStr.Split(FString(";"), &TempKeyValPair, &TempStr, ESearchCase::IgnoreCase, ESearchDir::FromStart))
	{
		FString l;
		FString r;

		if (TempKeyValPair.Split(FString(DisplayClusterStrings::common::KeyValSeparator), &l, &r, ESearchCase::IgnoreCase, ESearchDir::FromStart) == false)
		{
			return false;
		}

		Parameters.Add(l, r);
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////////////////////////
// FDisplayClusterClusterEventBinary
//////////////////////////////////////////////////////////////////////////////////////////////
void FDisplayClusterClusterEventBinary::SerializeToByteArray(TArray<uint8>& Arch) const
{
	// Allocate buffer memory
	const uint32 BufferSize = sizeof(EventId) + sizeof(bIsSystemEvent) + sizeof(bShouldDiscardOnRepeat) + EventData.Num();
	Arch.SetNumUninitialized(BufferSize);

	uint32 WriteOffset = 0;

	// EventId
	FMemory::Memcpy(Arch.GetData() + WriteOffset, &EventId, sizeof(EventId));
	WriteOffset += sizeof(EventId);

	// bIsSystemEvent
	FMemory::Memcpy(Arch.GetData() + WriteOffset, &bIsSystemEvent, sizeof(bIsSystemEvent));
	WriteOffset += sizeof(bIsSystemEvent);

	// bShouldDiscardOnRepeat
	FMemory::Memcpy(Arch.GetData() + WriteOffset, &bShouldDiscardOnRepeat, sizeof(bShouldDiscardOnRepeat));
	WriteOffset += sizeof(bShouldDiscardOnRepeat);

	// EventData
	FMemory::Memcpy(Arch.GetData() + WriteOffset, EventData.GetData(), EventData.Num());
}

bool FDisplayClusterClusterEventBinary::DeserializeFromByteArray(const TArray<uint8>& Arch)
{
	static const int32 MinBufferSize = sizeof(EventId) + sizeof(bIsSystemEvent) + sizeof(bShouldDiscardOnRepeat);

	if (Arch.Num() < MinBufferSize)
	{
		return false;
	}

	uint32 ReadOffset = 0;

	// EventId
	FMemory::Memcpy(&EventId, Arch.GetData() + ReadOffset, sizeof(EventId));
	ReadOffset += sizeof(EventId);

	// bIsSystemEvent
	FMemory::Memcpy(&bIsSystemEvent, Arch.GetData() + ReadOffset, sizeof(bIsSystemEvent));
	ReadOffset += sizeof(bIsSystemEvent);

	// bShouldDiscardOnRepeat
	FMemory::Memcpy(&bShouldDiscardOnRepeat, Arch.GetData() + ReadOffset, sizeof(bShouldDiscardOnRepeat));
	ReadOffset += sizeof(bShouldDiscardOnRepeat);

	// EventData
	const uint32 BinaryDataLength = Arch.Num() - ReadOffset;
	EventData.AddUninitialized(BinaryDataLength);
	FMemory::Memcpy(EventData.GetData(), Arch.GetData() + ReadOffset, BinaryDataLength);

	return true;
}
