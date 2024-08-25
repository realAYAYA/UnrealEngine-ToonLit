// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGMergeAttributes.h"

#include "PCGContext.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Metadata/PCGMetadata.h"

#define LOCTEXT_NAMESPACE "PCGMergeAttributesSettings"

#if WITH_EDITOR
FText UPCGMergeAttributesSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Merge Attributes");
}

FText UPCGMergeAttributesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Merges multiple attribute sets in a single attribute set with multiple entries and all the provided attributes");
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGMergeAttributesSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Param, /*bAllowMultipleConnections=*/true);
	InputPinProperty.SetRequiredPin();

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGMergeAttributesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param, /*bAllowMultipleConnections=*/true, /*bAllowMultipleData=*/false);

	return PinProperties;
}

FPCGElementPtr UPCGMergeAttributesSettings::CreateElement() const
{
	return MakeShared<FPCGMergeAttributesElement>();
}

bool FPCGMergeAttributesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMergeAttributesElement::Execute);
	check(Context);

	const UPCGMergeAttributesSettings* Settings = Context->GetInputSettings<UPCGMergeAttributesSettings>();
	check(Settings);

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	FPCGTaggedData* MergedOutput = nullptr;
	UPCGParamData* MergedAttributeSet = nullptr;

	for (const FPCGTaggedData& Source : Context->InputData.TaggedData)
	{
		const UPCGParamData* SourceData = Cast<const UPCGParamData>(Source.Data);

		if (!SourceData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnsupportedDataType", "Unsupported data type in merge attributes"));
			continue;
		}

		const UPCGMetadata* SourceMetadata = SourceData->Metadata;
		const int64 ParamItemCount = SourceData->Metadata->GetLocalItemCount();

		if (ParamItemCount <= 0)
		{
			continue;
		}

		if (!MergedOutput)
		{
			MergedOutput = &Outputs.Add_GetRef(Source);
			continue;
		}
		
		// When we're merging the 2nd element, create the actual merged attribute set
		if (!MergedAttributeSet)
		{
			MergedAttributeSet = NewObject<UPCGParamData>();
			check(MergedOutput);
			MergedAttributeSet->Metadata->InitializeAsCopy(CastChecked<const UPCGParamData>(MergedOutput->Data)->Metadata);
			MergedOutput->Data = MergedAttributeSet;
		}

		// For all entries starting from the second:
		// - Add missing attributes
		MergedAttributeSet->Metadata->AddAttributes(SourceMetadata);

		// - Merge entries
		TArray<PCGMetadataEntryKey, TInlineAllocator<256>> SourceEntryKeys;
		SourceEntryKeys.SetNumUninitialized(ParamItemCount);
		for (int64 LocalItemKey = 0; LocalItemKey < ParamItemCount; ++LocalItemKey)
		{
			SourceEntryKeys[LocalItemKey] = LocalItemKey;
		}

		TArray<PCGMetadataEntryKey, TInlineAllocator<256>> EntryKeys;
		EntryKeys.Init(PCGInvalidEntryKey, ParamItemCount);

		MergedAttributeSet->Metadata->SetAttributes(SourceEntryKeys, SourceMetadata, EntryKeys, Context);

		// - Merge tags too (to be in line with the Merge points node)
		MergedOutput->Tags.Append(Source.Tags);
	}

	return true;
}

#undef LOCTEXT_NAMESPACE