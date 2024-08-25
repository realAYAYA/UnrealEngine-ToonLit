// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGSortAttributes.h"

#include "PCGContext.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#define LOCTEXT_NAMESPACE "PCGSortAttributesElement"

#if WITH_EDITOR
FName UPCGSortAttributesSettings::GetDefaultNodeName() const
{
	return FName(TEXT("SortAttributes"));
}

FText UPCGSortAttributesSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Sort Attributes");
}

FText UPCGSortAttributesSettings::GetNodeTooltipText() const
{
	return LOCTEXT("NodeTooltip", "Sorts data based on an attribute.");
}

TArray<FText> UPCGSortAttributesSettings::GetNodeTitleAliases() const
{
	// Re-use old LOCTEXT name if it was already localized.
	return { NSLOCTEXT("PCGSortPointsElement", "NodeTitle", "Sort Points") };
}
#endif // WITH_EDITOR

TArray<FPCGPinProperties> UPCGSortAttributesSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Any);

	return PinProperties;
}

FPCGElementPtr UPCGSortAttributesSettings::CreateElement() const
{
	return MakeShared<FPCGSortAttributesElement>();
}

bool FPCGSortAttributesElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSortAttributesElement::Execute);
	
	check(Context);

	const UPCGSortAttributesSettings* Settings = Context->GetInputSettings<UPCGSortAttributesSettings>();
	check(Settings);
	
	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (int32 i = 0; i < Inputs.Num(); ++i)
	{
		TRACE_CPUPROFILER_EVENT_SCOPE(FPCGSortAttributesElement::InputLoop);

		const FPCGTaggedData& Input = Inputs[i];

		if (const UPCGData* InputData = Input.Data)
		{
			FPCGAttributePropertyInputSelector InputSource = Settings->InputSource.CopyAndFixLast(InputData);
			TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputData, InputSource);
			TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(InputData, InputSource);

			if (!Accessor.IsValid() || !Keys.IsValid())
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidAccessor", "Attribute '{0}' does not exist for input {1}."), InputSource.GetDisplayText(), FText::AsNumber(i)));
				continue;
			}

			if (!PCGMetadataAttribute::CallbackWithRightType(Accessor->GetUnderlyingType(), [](auto Dummy) -> bool { return PCG::Private::MetadataTraits<decltype(Dummy)>::CanCompare; }))
			{
				PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InvalidTypeAccessor", "Attribute '{0}' exists but is not of a comparable type ({1}) for input {2}."), InputSource.GetDisplayText(), PCG::Private::GetTypeNameText(Accessor->GetUnderlyingType()), FText::AsNumber(i)));
				continue;
			}

			UPCGData* OutputData = nullptr;
			if (const UPCGPointData* InputPointData = Cast<const UPCGPointData>(InputData))
			{
				UPCGPointData* OutputPointData = static_cast<UPCGPointData*>(InputData->DuplicateData());
				PCGAttributeAccessorHelpers::SortByAttribute(*Accessor, *Keys, OutputPointData->GetMutablePoints(), Settings->SortMethod == EPCGSortMethod::Ascending);
				OutputData = OutputPointData;
			}
			else if (const UPCGMetadata* InMetadata = InputData->ConstMetadata())
			{
				// Duplicate data without metadata
				OutputData = InputData->DuplicateData(/*bInitializeMetdata=*/false);
				UPCGMetadata* OutMetadata = OutputData->MutableMetadata();

				if (!ensure(OutMetadata))
				{
					continue;
				}

				// Gather all the entries and sort them
				TArray<PCGMetadataEntryKey> Entries;
				const int64 Count = InMetadata->GetItemCountForChild();

				Entries.Reserve(Count);

				for (PCGMetadataEntryKey Entry = 0; Entry < Count; ++Entry)
				{
					Entries.Add(Entry);
				}

				PCGAttributeAccessorHelpers::SortByAttribute(*Accessor, *Keys, Entries, Settings->SortMethod == EPCGSortMethod::Ascending);

				OutMetadata->InitializeAsCopy(InputData->ConstMetadata(), &Entries);
			}

			FPCGTaggedData& Output = Outputs.Add_GetRef(Input);
			Output.Data = OutputData;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
