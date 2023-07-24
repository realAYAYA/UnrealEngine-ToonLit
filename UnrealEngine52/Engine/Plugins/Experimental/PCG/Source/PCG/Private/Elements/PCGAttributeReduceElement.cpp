// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGAttributeReduceElement.h"

#include "Data/PCGSpatialData.h"
#include "PCGParamData.h"
#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "PCGContext.h"
#include "PCGPin.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGAttributeReduceElement)

#define LOCTEXT_NAMESPACE "PCGAttributeReduceElement"

namespace PCGAttributeReduceElement
{
	template <typename T>
	bool Average(const IPCGAttributeAccessorKeys& Keys, const IPCGAttributeAccessor& Accessor, T& OutValue)
	{
		if constexpr (!PCG::Private::MetadataTraits<T>::CanSubAdd || !PCG::Private::MetadataTraits<T>::CanInterpolate)
		{
			return false;
		}
		else
		{
			OutValue = PCG::Private::MetadataTraits<T>::ZeroValue();

			bool bSuccess = PCGMetadataElementCommon::ApplyOnAccessor<T>(Keys, Accessor, [&OutValue](const T& InValue, int32)
				{
					OutValue = PCG::Private::MetadataTraits<T>::Add(OutValue, InValue);
				});

			if (bSuccess)
			{
				OutValue = PCG::Private::MetadataTraits<T>::WeightedSum(PCG::Private::MetadataTraits<T>::ZeroValue(), OutValue, 1.0f / Keys.GetNum());
			}

			return bSuccess;
		}
	}

	template <typename T, bool bIsMin>
	bool MinMax(const IPCGAttributeAccessorKeys& Keys, const IPCGAttributeAccessor& Accessor, T& OutValue)
	{
		if constexpr (!PCG::Private::MetadataTraits<T>::CanMinMax)
		{
			return false;
		}
		else
		{
			OutValue = PCG::Private::MetadataTraits<T>::ZeroValue();
			bool bFirstValue = true;

			return PCGMetadataElementCommon::ApplyOnAccessor<T>(Keys, Accessor, [&OutValue, &bFirstValue](const T& InValue, int32)
				{
					if (bFirstValue)
					{
						OutValue = InValue;
						bFirstValue = false;
					}
					else
					{
						if constexpr (bIsMin)
						{
							OutValue = PCG::Private::MetadataTraits<T>::Min(OutValue, InValue);
						}
						else
						{
							OutValue = PCG::Private::MetadataTraits<T>::Max(OutValue, InValue);
						}
					}
				});
		}
	}
}

#if WITH_EDITOR
FName UPCGAttributeReduceSettings::GetDefaultNodeName() const
{
	return TEXT("AttributeReduce");
}

FText UPCGAttributeReduceSettings::GetDefaultNodeTitle() const
{
	return LOCTEXT("NodeTitle", "Attribute Reduce");
}
#endif

void UPCGAttributeReduceSettings::PostLoad()
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

FName UPCGAttributeReduceSettings::AdditionalTaskName() const
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGAttributeReduceOperation>())
	{
		const FString OperationName = EnumPtr->GetNameStringByValue(static_cast<int>(Operation));

		FName InputAttributeName = InputSource.GetName();
		if (InputAttributeName == NAME_None)
		{
			InputAttributeName = FName(TEXT("LastAttribute"));
		}

		if (InputAttributeName != OutputAttributeName && OutputAttributeName != NAME_None)
		{
			return FName(FString::Printf(TEXT("Reduce %s to %s: %s"), *InputAttributeName.ToString(), *OutputAttributeName.ToString(), *OperationName));
		}
		else
		{
			return FName(FString::Printf(TEXT("Reduce %s: %s"), *InputAttributeName.ToString(), *OperationName));
		}
	}
	else
	{
		return NAME_None;
	}
}

TArray<FPCGPinProperties> UPCGAttributeReduceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Spatial, /*bInAllowMultipleConnections=*/ false);

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGAttributeReduceSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultOutputLabel, EPCGDataType::Param);

	return PinProperties;
}

FPCGElementPtr UPCGAttributeReduceSettings::CreateElement() const
{
	return MakeShared<FPCGAttributeReduceElement>();
}

bool FPCGAttributeReduceElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGAttributeReduceElement::Execute);

	check(Context);

	const UPCGAttributeReduceSettings* Settings = Context->GetInputSettings<UPCGAttributeReduceSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputsByPin(PCGPinConstants::DefaultInputLabel);

	if (Inputs.Num() != 1)
	{
		PCGE_LOG(Warning, LogOnly, FText::Format(LOCTEXT("WrongNumberOfInputs", "Input pin expected to have one input data element, encountered {0}"), Inputs.Num()));
		return true;
	}

	const UPCGSpatialData* SpatialData = Cast<UPCGSpatialData>(Inputs[0].Data);

	if (!SpatialData)
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("InputNotSpatialData", "Input is not a spatial data"));
		return true;
	}

	const UPCGPointData* PointData = Cast<UPCGPointData>(SpatialData);

	FPCGAttributePropertySelector InputSource = Settings->InputSource;
	if (InputSource.Selection == EPCGAttributePropertySelection::Attribute && InputSource.AttributeName == NAME_None && SpatialData->Metadata)
	{
		InputSource.SetAttributeName(SpatialData->Metadata->GetLatestAttributeNameOrNone());
	}

	const FName OutputAttributeName = (Settings->OutputAttributeName == NAME_None) ? InputSource.GetName() : Settings->OutputAttributeName;
	UPCGParamData* OutputParamData = NewObject<UPCGParamData>();

	TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(PointData, InputSource);
	TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(PointData, InputSource);

	if (!Accessor.IsValid() || !Keys.IsValid())
	{
		PCGE_LOG(Error, GraphAndLog, LOCTEXT("AttributeDoesNotExist", "Input attribute/property does not exist"));
		return true;
	}

	auto DoOperation = [&Accessor, &Keys, Operation = Settings->Operation, OutputParamData, OutputAttributeName](auto DummyValue) -> bool
	{
		using AttributeType = decltype(DummyValue);

		bool bSuccess = false;

		AttributeType OutputValue{};

		FPCGMetadataAttribute<AttributeType>* NewAttribute = static_cast<FPCGMetadataAttribute<AttributeType>*>(
			OutputParamData->Metadata->CreateAttribute<AttributeType>(OutputAttributeName, OutputValue, /*bAllowInterpolation=*/ true, /*bOverrideParent=*/false));

		if (!NewAttribute)
		{
			return false;
		}

		switch (Operation)
		{
		case EPCGAttributeReduceOperation::Average:
			bSuccess = PCGAttributeReduceElement::Average<AttributeType>(*Keys, *Accessor, OutputValue);
			break;
		case EPCGAttributeReduceOperation::Max:
			bSuccess = PCGAttributeReduceElement::MinMax<AttributeType, /*bIsMin*/false>(*Keys, *Accessor, OutputValue);
			break;
		case EPCGAttributeReduceOperation::Min:
			bSuccess = PCGAttributeReduceElement::MinMax<AttributeType, /*bIsMin*/true>(*Keys, *Accessor, OutputValue);
			break;
		default:
			break;
		}

		if (bSuccess)
		{
			NewAttribute->SetDefaultValue(OutputValue);
			NewAttribute->SetValue(OutputParamData->Metadata->AddEntry(), OutputValue);
		}

		return bSuccess;
	};

	if (!PCGMetadataAttribute::CallbackWithRightType(Accessor->GetUnderlyingType(), DoOperation))
	{
		PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeOperationFailed", "Operation was not compatible with the attribute type or could not create attribute '{0}'"), FText::FromName(OutputAttributeName)));
		return true;
	}

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	FPCGTaggedData& Output = Outputs.Emplace_GetRef();
	Output.Data = OutputParamData;

	return true;
}

#undef LOCTEXT_NAMESPACE
