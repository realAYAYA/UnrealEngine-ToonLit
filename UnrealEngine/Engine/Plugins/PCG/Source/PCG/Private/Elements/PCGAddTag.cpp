// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGAddTag.h"

#include "PCGContext.h"
#include "PCGPin.h"
#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGAddTagElement"

TArray<FPCGPinProperties> UPCGAddTagSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGAddTagSettings::CreateElement() const
{
	return MakeShared<FPCGAddTagElement>();
}

bool FPCGAddTagElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAddTagElement::Execute);
	
	check(Context);

	const UPCGAddTagSettings* Settings = Context->GetInputSettings<UPCGAddTagSettings>();
	check(Settings);
	
	Context->OutputData.TaggedData = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	const TArray<FString> TagsArray = PCGHelpers::GetStringArrayFromCommaSeparatedString(Settings->TagsToAdd);
	
	for (const FString& Tag : TagsArray)
	{
		for (FPCGTaggedData& OutputTaggedData : Context->OutputData.TaggedData)
		{
				OutputTaggedData.Tags.Add(Tag);
		}
	}
	
	return true;
}

#undef LOCTEXT_NAMESPACE
