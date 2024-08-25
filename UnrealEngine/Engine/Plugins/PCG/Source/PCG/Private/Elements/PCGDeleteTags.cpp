// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGDeleteTags.h"

#include "PCGContext.h"
#include "PCGData.h"
#include "Helpers/PCGHelpers.h"

#define LOCTEXT_NAMESPACE "PCGDeleteTagsElement"

#if WITH_EDITOR
FName UPCGDeleteTagsSettings::GetDefaultNodeName() const
{
	return TEXT("DeleteTags");
}

FText UPCGDeleteTagsSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Delete Tags");
}
#endif // WITH_EDITOR

FString UPCGDeleteTagsSettings::GetAdditionalTitleInformation() const
{
	if (const UEnum* SelectionEnum = StaticEnum<EPCGTagFilterOperation>())
	{
		FText OperationText = SelectionEnum->GetDisplayNameTextByValue(static_cast<int64>(Operation));
		TArray<FString> TagsToProcess = PCGHelpers::GetStringArrayFromCommaSeparatedString(SelectedTags);

		if (TagsToProcess.Num() == 1)
		{
			return FString::Printf(TEXT("%s (%s)"), *OperationText.ToString(), *TagsToProcess[0]);
		}
		else
		{
			return OperationText.ToString();
		}
	}
	else
	{
		return FString();
	}
}

TArray<FPCGPinProperties> UPCGDeleteTagsSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGDeleteTagsSettings::CreateElement() const
{
	return MakeShared<FPCGDeleteTagsElement>();
}

bool FPCGDeleteTagsElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGManageTagsElement::Execute);
	check(Context);

	const UPCGDeleteTagsSettings* Settings = Context->GetInputSettings<UPCGDeleteTagsSettings>();
	check(Settings);

	const TArray<FString> FilterTags = PCGHelpers::GetStringArrayFromCommaSeparatedString(Settings->SelectedTags);
	TSet<FString> TagsToFilter(FilterTags);

	const bool bKeepInFilter = (Settings->Operation == EPCGTagFilterOperation::KeepOnlySelectedTags);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		Output.Tags.Reset();
		Output.Tags.Reserve(Input.Tags.Num());

		for (const FString& Tag : Input.Tags)
		{
			if (TagsToFilter.Contains(Tag) == bKeepInFilter)
			{
				Output.Tags.Add(Tag);
			}
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE