// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/DataStream/DataStreamDefinitions.h"
#include "Iris/DataStream/DataStream.h"
#include "Iris/Core/IrisLog.h"
#include "UObject/UObjectGlobals.h"

UDataStreamDefinitions::UDataStreamDefinitions()
: bFixupComplete(false)
{
}

void UDataStreamDefinitions::FixupDefinitions()
{
	if (bFixupComplete)
	{
		return;
	}

	for (FDataStreamDefinition& Definition : DataStreamDefinitions)
	{
		UE_CLOG(DataStreamDefinitions.ContainsByPredicate([Name = Definition.DataStreamName, &Definition](const FDataStreamDefinition& ExistingDefinition) { return Name == ExistingDefinition.DataStreamName && &Definition != &ExistingDefinition; }), LogIris, Error, TEXT("DataStream name is defined multiple times: %s."), *Definition.DataStreamName.GetPlainNameString());
		UE_CLOG(!StaticEnum<EDataStreamSendStatus>()->IsValidEnumValue(int8(Definition.DefaultSendStatus)), LogIris, Error, TEXT("Invalid DataStreamSendStatus %u for DataStream %s."), unsigned(Definition.DefaultSendStatus), *Definition.DataStreamName.GetPlainNameString());

		Definition.Class = StaticLoadClass(UDataStream::StaticClass(), nullptr, *Definition.ClassName.ToString(), nullptr, LOAD_Quiet);

		UE_CLOG(Definition.Class == nullptr, LogIris, Error, TEXT("DataStream class could not be loaded: %s"), *Definition.ClassName.GetPlainNameString());
	}

	bFixupComplete = true;
}

const FDataStreamDefinition* UDataStreamDefinitions::FindDefinition(const FName Name) const
{
	return DataStreamDefinitions.FindByPredicate([Name](const FDataStreamDefinition& Definition) { return Name == Definition.DataStreamName; });
}

void UDataStreamDefinitions::GetAutoCreateStreamNames(TArray<FName>& OutStreamNames) const
{
	for (const FDataStreamDefinition& Definition : DataStreamDefinitions)
	{
		if (Definition.bAutoCreate)
		{
			OutStreamNames.Add(Definition.DataStreamName);
		}
	}
}
