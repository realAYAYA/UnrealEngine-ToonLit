// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGAttributeGetFromPointIndexElement.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGPin.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGAttributeGetFromPointIndexElement)

#define LOCTEXT_NAMESPACE "PCGAttributeGetFromPointIndexElement"

#if WITH_EDITOR
FName UPCGAttributeGetFromPointIndexSettings::GetDefaultNodeName() const
{
	return TEXT("GetAttributeFromPointIndex");
}

FText UPCGAttributeGetFromPointIndexSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Get Attribute From Point Index");
}

void UPCGAttributeGetFromPointIndexSettings::ApplyDeprecation(UPCGNode* InOutNode)
{
	if (DataVersion < FPCGCustomVersion::UpdateAttributePropertyInputSelector
		&& (OutputAttributeName == NAME_None))
	{
		// Previous behavior of the output attribute for this node was:
		// None => SameName
		OutputAttributeName = PCGMetadataAttributeConstants::SourceNameAttributeName;
	}

	Super::ApplyDeprecation(InOutNode);
}
#endif

void UPCGAttributeGetFromPointIndexSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (InputAttributeName_DEPRECATED != NAME_None)
	{
		InputSource.SetAttributeName(InputAttributeName_DEPRECATED);
		InputAttributeName_DEPRECATED = NAME_None;
	}
#endif
}

TArray<FPCGPinProperties> UPCGAttributeGetFromPointIndexSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	FPCGPinProperties& InputPinProperty = PinProperties.Emplace_GetRef(PCGPinConstants::DefaultInputLabel, EPCGDataType::Point);
	InputPinProperty.SetRequiredPin();

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGAttributeGetFromPointIndexSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGAttributeGetFromPointIndexConstants::OutputAttributeLabel, EPCGDataType::Param);
	PinProperties.Emplace(PCGAttributeGetFromPointIndexConstants::OutputPointLabel, EPCGDataType::Point);

	return PinProperties;
}

FPCGElementPtr UPCGAttributeGetFromPointIndexSettings::CreateElement() const
{
	return MakeShared<FPCGAttributeGetFromPointIndexElement>();
}

bool FPCGAttributeGetFromPointIndexElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAttributeGetFromPointIndexElement::Execute);

	check(Context);

	const UPCGAttributeGetFromPointIndexSettings* Settings = Context->GetInputSettings<UPCGAttributeGetFromPointIndexSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	for (int32 InputIndex = 0; InputIndex < Inputs.Num(); ++InputIndex)
	{
		const FPCGTaggedData& Input = Inputs[InputIndex];
		const UPCGPointData* PointData = Cast<UPCGPointData>(Input.Data);

		if (!PointData)
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("InputNotPointData", "Input {0} is not a point data"), InputIndex));
			continue;
		}

		const int32 Index = Settings->Index;

		if (Index < 0 || Index >= PointData->GetPoints().Num())
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("IndexOutOfBounds", "Index for input {0} is out of bounds. Index: {1}; Number of Points: {2}"), InputIndex, Index, PointData->GetPoints().Num()));
			continue;
		}

		TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
		const FPCGPoint& Point = PointData->GetPoints()[Index];

		FPCGAttributePropertyInputSelector InputSource = Settings->InputSource.CopyAndFixLast(PointData);

		const FName OutputAttributeName = (Settings->OutputAttributeName == PCGMetadataAttributeConstants::SourceNameAttributeName) ? InputSource.GetName() : Settings->OutputAttributeName;

		TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(PointData, InputSource);
		FPCGAttributeAccessorKeysPoints PointKey(Point);

		if (Accessor.IsValid())
		{
			UPCGParamData* OutputParamData = NewObject<UPCGParamData>();

			auto ExtractAttribute = [this, Context, OutputAttributeName, &OutputParamData, &Accessor, &PointKey, InputIndex](auto DummyValue) -> bool
			{
				using AttributeType = decltype(DummyValue);

				AttributeType Value{};

				// Should never fail, as OutputType == Accessor->UnderlyingType
				if (!ensure(Accessor->Get<AttributeType>(Value, PointKey)))
				{
					return false;
				}

				FPCGMetadataAttribute<AttributeType>* NewAttribute = static_cast<FPCGMetadataAttribute<AttributeType>*>(
					OutputParamData->Metadata->CreateAttribute<AttributeType>(OutputAttributeName, Value, /*bAllowInterpolation=*/true, /*bOverrideParent=*/false));

				if (!NewAttribute)
				{
					PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("ErrorCreatingTargetAttribute", "Error while creating target attribute '{0}' for output {1}"), FText::FromName(OutputAttributeName), InputIndex));
					return false;
				}

				OutputParamData->Metadata->AddEntry();

				return true;
			};

			if (PCGMetadataAttribute::CallbackWithRightType(Accessor->GetUnderlyingType(), ExtractAttribute))
			{
#if !WITH_EDITOR
				// Add the point
				// Eschew output creation only in non-editor builds
				if (Context->Node && Context->Node->IsOutputPinConnected(PCGAttributeGetFromPointIndexConstants::OutputPointLabel))
#endif
				{
					UPCGPointData* OutputPointData = NewObject<UPCGPointData>();
					OutputPointData->InitializeFromData(PointData);
					OutputPointData->GetMutablePoints().Add(Point);

					FPCGTaggedData& OutputPoint = Outputs.Add_GetRef(Input);
					OutputPoint.Data = OutputPointData;
					OutputPoint.Pin = PCGAttributeGetFromPointIndexConstants::OutputPointLabel;
				}

				// And the attribute
				FPCGTaggedData& OutputAttribute = Outputs.Add_GetRef(Input);
				OutputAttribute.Data = OutputParamData;
				OutputAttribute.Pin = PCGAttributeGetFromPointIndexConstants::OutputAttributeLabel;
			}
		}
		else
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeNotFound", "Cannot find attribute/property '{0}' in input {1}"), FText::FromName(InputSource.GetName()), InputIndex));
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
