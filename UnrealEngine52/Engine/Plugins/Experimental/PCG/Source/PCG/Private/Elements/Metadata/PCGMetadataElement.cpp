// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataElement.h"

#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "PCGContext.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGMetadataElement)

#define LOCTEXT_NAMESPACE "PCGMetadataElement"

UPCGMetadataOperationSettings::UPCGMetadataOperationSettings()
{
	// Previous default object was: None for source and target attribute, density for point property
	// and Property to attribute
	// Recreate the same default
	InputSource.SetPointProperty(EPCGPointProperties::Density);
	OutputTarget.SetAttributeName(NAME_None);
}

FPCGElementPtr UPCGMetadataOperationSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataOperationElement>();
}

void UPCGMetadataOperationSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if (Target_DEPRECATED != EPCGMetadataOperationTarget::PropertyToAttribute
		|| (SourceAttribute_DEPRECATED != NAME_None)
		|| (DestinationAttribute_DEPRECATED != NAME_None)
		|| (PointProperty_DEPRECATED != EPCGPointProperties::Density))
	{
		switch (Target_DEPRECATED)
		{
		case EPCGMetadataOperationTarget::PropertyToAttribute:
			InputSource.SetPointProperty(PointProperty_DEPRECATED);
			OutputTarget.SetAttributeName(DestinationAttribute_DEPRECATED);
			break;
		case EPCGMetadataOperationTarget::AttributeToProperty:
			InputSource.SetAttributeName(SourceAttribute_DEPRECATED);
			OutputTarget.SetPointProperty(PointProperty_DEPRECATED);
			break;
		case EPCGMetadataOperationTarget::AttributeToAttribute:
			InputSource.SetAttributeName(SourceAttribute_DEPRECATED);
			OutputTarget.SetAttributeName(DestinationAttribute_DEPRECATED);
			break;
		default:
			break;
		}

		// Default values.
		SourceAttribute_DEPRECATED = NAME_None;
		DestinationAttribute_DEPRECATED = NAME_None;
		Target_DEPRECATED = EPCGMetadataOperationTarget::PropertyToAttribute;
		PointProperty_DEPRECATED = EPCGPointProperties::Density;
	}
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR
}

bool FPCGMetadataOperationElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataOperationElement::Execute);

	const UPCGMetadataOperationSettings* Settings = Context->GetInputSettings<UPCGMetadataOperationSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Input.Data);

		if (!SpatialInput)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("InvalidInputData", "Invalid input data type, must be of type Spatial"));
			continue;
		}

		const UPCGPointData* OriginalData = SpatialInput->ToPointData(Context);

		if (!OriginalData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("UnableToGetPointData", "Unable to get point data from input"));
			continue;
		}

		if (!OriginalData->Metadata)
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("MissingMetadata", "Input has no metadata"));
			continue;
		}

		FPCGAttributePropertySelector InputSource = Settings->InputSource;
		FPCGAttributePropertySelector OutputTarget = Settings->OutputTarget;

		const FName SourceAttribute = InputSource.GetName();
		const FName DestinationAttribute = OutputTarget.GetName();
		const FName LocalSourceAttribute = ((SourceAttribute != NAME_None) ? SourceAttribute : OriginalData->Metadata->GetLatestAttributeNameOrNone());
		const FName LocalDestinationAttribute = ((DestinationAttribute != NAME_None) ? DestinationAttribute : OriginalData->Metadata->GetLatestAttributeNameOrNone());

		// Make sure we use the right attribute name
		InputSource.AttributeName = LocalSourceAttribute;
		OutputTarget.AttributeName = LocalDestinationAttribute;

		if (Settings->InputSource.Selection == EPCGAttributePropertySelection::Attribute && !OriginalData->Metadata->HasAttribute(LocalSourceAttribute))
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("InputMissingAttribute", "Input does not have the '{0}' attribute"), FText::FromName(LocalSourceAttribute)));
			continue;
		}

		const TArray<FPCGPoint>& Points = OriginalData->GetPoints();
		const int OriginalPointCount = Points.Num();

		UPCGPointData* SampledData = NewObject<UPCGPointData>();
		SampledData->InitializeFromData(OriginalData);
		TArray<FPCGPoint>& SampledPoints = SampledData->GetMutablePoints();

		Output.Data = SampledData;

		// Copy points and then apply the operation
		SampledPoints = Points;

		// If it is attribute to attribute, just copy the attributes, if they exist and are valid
		if (Settings->InputSource.Selection == EPCGAttributePropertySelection::Attribute && OutputTarget.Selection == EPCGAttributePropertySelection::Attribute)
		{
			if (LocalSourceAttribute == DestinationAttribute)
			{
				// Nothing to do...
				continue;
			}

			if (!SampledData->Metadata->CopyExistingAttribute(LocalSourceAttribute, DestinationAttribute))
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("FailedCopyToNewAttribute", "Failed to copy to new attribute {0}"), FText::FromName(DestinationAttribute)));
			}

			continue;
		}

		TUniquePtr<const IPCGAttributeAccessor> InputAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(OriginalData, InputSource);
		TUniquePtr<const IPCGAttributeAccessorKeys> InputKeys = PCGAttributeAccessorHelpers::CreateConstKeys(OriginalData, InputSource);

		if (!InputAccessor.IsValid() || !InputKeys.IsValid())
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("FailedToCreateInputAccessor", "Failed to create input accessor or iterator"));
			continue;
		}

		// If the target is an attribute, only create a new one if the attribute doesn't already exist.
		// If it exist, it will try to write to it.
		if (OutputTarget.Selection == EPCGAttributePropertySelection::Attribute && !OriginalData->Metadata->HasAttribute(LocalDestinationAttribute))
		{
			auto CreateAttribute = [SampledData, LocalDestinationAttribute](auto Dummy)
			{
				using AttributeType = decltype(Dummy);
				return PCGMetadataElementCommon::ClearOrCreateAttribute(SampledData->Metadata, LocalDestinationAttribute, AttributeType{}) != nullptr;
			};
			
			if (!PCGMetadataAttribute::CallbackWithRightType(InputAccessor->GetUnderlyingType(), CreateAttribute))
			{
				PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("FailedToCreateNewAttribute", "Failed to create new attribute {0}"), FText::FromName(LocalDestinationAttribute)));
				continue;
			}
		}

		TUniquePtr<IPCGAttributeAccessor> OutputAccessor = PCGAttributeAccessorHelpers::CreateAccessor(SampledData, Settings->OutputTarget);
		TUniquePtr<IPCGAttributeAccessorKeys> OutputKeys = PCGAttributeAccessorHelpers::CreateKeys(SampledData, Settings->OutputTarget);

		if (!OutputAccessor.IsValid() || !OutputKeys.IsValid())
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("FailedToCreateOutputAccessor", "Failed to create output accessor or iterator"));
			continue;
		}

		// By construction, they should be the same
		check(InputKeys->GetNum() == OutputKeys->GetNum());

		// Final verification, if we can put the value of input into output
		if (!PCG::Private::IsBroadcastable(InputAccessor->GetUnderlyingType(), OutputAccessor->GetUnderlyingType()))
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotBroadcastTypes", "Cannot broadcast input type into output type"));
			continue;
		}

		// At this point, we are ready.
		auto Operation = [&InputAccessor, &InputKeys, &OutputAccessor, &OutputKeys](auto Dummy)
		{
			using OutputType = decltype(Dummy);
			OutputType Value{};

			EPCGAttributeAccessorFlags Flags = EPCGAttributeAccessorFlags::AllowBroadcast;

			const int32 NumberOfElements = InputKeys->GetNum();
			constexpr int32 ChunkSize = 256;

			TArray<OutputType, TInlineAllocator<ChunkSize>> TempValues;
			TempValues.SetNum(ChunkSize);

			const int32 NumberOfIterations = (NumberOfElements + ChunkSize - 1) / ChunkSize;

			for (int32 i = 0; i < NumberOfIterations; ++i)
			{
				const int32 StartIndex = i * ChunkSize;
				const int32 Range = FMath::Min(NumberOfElements - StartIndex, ChunkSize);
				TArrayView<OutputType> View(TempValues.GetData(), Range);

				InputAccessor->GetRange<OutputType>(View, StartIndex, *InputKeys, Flags);
				OutputAccessor->SetRange<OutputType>(View, StartIndex, *OutputKeys, Flags);
			}

			return true;
		};

		if (!PCGMetadataAttribute::CallbackWithRightType(OutputAccessor->GetUnderlyingType(), Operation))
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("ErrorGettingSettingValues", "Error while getting/setting values"));
			continue;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
