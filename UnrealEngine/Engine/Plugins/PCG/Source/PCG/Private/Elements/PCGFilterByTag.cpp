// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGFilterByTag.h"
#include "PCGContext.h"
#include "PCGPin.h"
#include "Helpers/PCGHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGFilterByTag)

#define LOCTEXT_NAMESPACE "PCGFilterByTag"

#if WITH_EDITOR
FName UPCGFilterByTagSettings::GetDefaultNodeName() const
{
	return FName(TEXT("FilterDataByTag"));
}

FText UPCGFilterByTagSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Filter Data By Tag");
}

FText UPCGFilterByTagSettings::GetNodeTooltipText() const
{
	return LOCTEXT("FilterByTagNodeTooltip", "Filters data in the collection according to whether they have, or don't have, some tags");
}
#endif

FString UPCGFilterByTagSettings::GetAdditionalTitleInformation() const
{
	const TArray<FString> Tags = PCGHelpers::GetStringArrayFromCommaSeparatedString(SelectedTags);

	const FString Prefix = (Operation == EPCGFilterByTagOperation::KeepTagged ? TEXT("Tag (Keep):") : TEXT("Tag (Remove):"));

	if (Tags.IsEmpty())
	{
		return Prefix;
	}
	else if (Tags.Num() == 1)
	{
		return FString::Printf(TEXT("%s %s"), *Prefix, *Tags[0]);
	}
	else
	{
		return FString::Printf(TEXT("%s (multiple)"), *Prefix);
	}
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

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
		Output.Pin = PCGPinConstants::DefaultOutFilterLabel;

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
			Output.Pin = PCGPinConstants::DefaultInFilterLabel;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE