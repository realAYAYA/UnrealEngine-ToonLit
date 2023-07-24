// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGFilterByTag.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Helpers/PCGHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGFilterByTag)

#define LOCTEXT_NAMESPACE "PCGFilterByTag"

namespace PCGFilterByTagConstants
{
	const FName NodeName = FName(TEXT("FilterByTag"));
	const FText NodeTitle = LOCTEXT("NodeTitle", "Filter By Tag");
}

#if WITH_EDITOR
FName UPCGFilterByTagSettings::GetDefaultNodeName() const
{
	return PCGFilterByTagConstants::NodeName;
}

FText UPCGFilterByTagSettings::GetDefaultNodeTitle() const
{
	return PCGFilterByTagConstants::NodeTitle;
}

FText UPCGFilterByTagSettings::GetNodeTooltipText() const
{
	return LOCTEXT("FilterByTagNodeTooltip", "Filters data in the collection according to whether they have, or don't have, some tags");
}
#endif

FName UPCGFilterByTagSettings::AdditionalTaskName() const
{
	const TArray<FString> Tags = PCGHelpers::GetStringArrayFromCommaSeparatedString(SelectedTags);

	FString NodeName = PCGFilterByTagConstants::NodeName.ToString();
	NodeName += (Operation == EPCGFilterByTagOperation::KeepTagged ? TEXT(" (Keep)") : TEXT(" (Remove)"));

	if (Tags.Num() == 1)
	{
		return FName(FString::Printf(TEXT("%s: %s"), *NodeName, *Tags[0]));
	}
	else
	{
		return FName(NodeName);
	}
}

TArray<FPCGPinProperties> UPCGFilterByTagSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGFilterByTagSettings::CreateElement() const
{
	return MakeShared<FPCGFilterByTagElement>();
}

bool FPCGFilterByTagElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGFilterByTagElement::Execute);
	check(Context);

	const UPCGFilterByTagSettings* Settings = Context->GetInputSettings<UPCGFilterByTagSettings>();

	const bool bKeepIfTag = (Settings->Operation == EPCGFilterByTagOperation::KeepTagged);
	const TArray<FString> Tags = PCGHelpers::GetStringArrayFromCommaSeparatedString(Settings->SelectedTags);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		bool bHasCommonTags = false;

		for (const FString& Tag : Tags)
		{
			if (Input.Tags.Contains(Tag))
			{
				bHasCommonTags = true;
				break;
			}
		}

		if (bKeepIfTag == bHasCommonTags)
		{
			Outputs.Add(Input);
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE