// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGAttributeReduceElement.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"

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
			if constexpr (std::is_same_v<FQuat, T>)
			{
				// Simple averaging will only work for similar quaternions, but the real answer needs a full solver with eigenvectors.
				FQuat ZeroQuat{ 0.0, 0.0, 0.0, 0.0 };
				OutValue = ZeroQuat;

				FQuat FirstQuat = ZeroQuat;
				const double Weight = 1.0f / Keys.GetNum();

				bool bSuccess = PCGMetadataElementCommon::ApplyOnAccessor<T>(Keys, Accessor, [&FirstQuat, Weight, &OutValue, &ZeroQuat](const T& InValue, int32 Index)
				{
					// Since doing a dot product with a quat equals to 0 will give us no info, we'll take the first non null quat.
					if (FirstQuat.Equals(ZeroQuat))
					{
						FirstQuat = InValue;
					}

					// Because q and -q represent the same quaternion, but the average would not be correct. We need to inverse the quaternion if needed.
					const double ThisWeight = (InValue | FirstQuat) < 0 ? -Weight : Weight;
					OutValue += (ThisWeight * InValue);
				});

				if (bSuccess)
				{
					OutValue.Normalize();
				}

				return bSuccess;
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

	template<typename T>
	bool Sum(const IPCGAttributeAccessorKeys& Keys, const IPCGAttributeAccessor& Accessor, T& OutValue)
	{
		if constexpr (!PCG::Private::MetadataTraits<T>::CanSubAdd)
		{
			return false;
		}
		else
		{
			OutValue = PCG::Private::MetadataTraits<T>::ZeroValue();

			return PCGMetadataElementCommon::ApplyOnAccessor<T>(Keys, Accessor, [&OutValue](const T& InValue, int32)
			{
				OutValue = PCG::Private::MetadataTraits<T>::Add(OutValue, InValue);
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

void UPCGAttributeReduceSettings::ApplyDeprecation(UPCGNode* InOutNode)
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

TArray<FPCGPreConfiguredSettingsInfo> UPCGAttributeReduceSettings::GetPreconfiguredInfo() const
{
	return PCGMetadataElementCommon::FillPreconfiguredSettingsInfoFromEnum<EPCGAttributeReduceOperation>();
}
#endif

void UPCGAttributeReduceSettings::ApplyPreconfiguredSettings(const FPCGPreConfiguredSettingsInfo& PreconfiguredInfo)
{
	if (const UEnum* EnumPtr = StaticEnum<EPCGAttributeReduceOperation>())
	{
		if (EnumPtr->IsValidEnumValue(PreconfiguredInfo.PreconfiguredIndex))
		{
			Operation = EPCGAttributeReduceOperation(PreconfiguredInfo.PreconfiguredIndex);
		}
	}
}

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

FString UPCGAttributeReduceSettings::GetAdditionalTitleInformation() const
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
			return FString::Printf(TEXT("Reduce %s to %s: %s"), *InputAttributeName.ToString(), *OutputAttributeName.ToString(), *OperationName);
		}
		else
		{
			return FString::Printf(TEXT("Reduce %s: %s"), *InputAttributeName.ToString(), *OperationName);
		}
	}
	else
	{
		return FString();
	}
}

TArray<FPCGPinProperties> UPCGAttributeReduceSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPinConstants::DefaultInputLabel, EPCGDataType::Any);

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
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	UPCGParamData* OutputParams = nullptr;
	FPCGMetadataAttributeBase* NewAttribute = nullptr;

	for (int32 i = 0; i < Inputs.Num(); ++i)
	{
		const UPCGData* InputData = Inputs[i].Data;

		if (!InputData)
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("InputNotSpatialData", "Input {0} is invalid, skipped"), FText::AsNumber(i)));
			continue;
		}

		FPCGAttributePropertyInputSelector InputSource = Settings->InputSource.CopyAndFixLast(InputData);

		const FName OutputAttributeName = (Settings->OutputAttributeName == PCGMetadataAttributeConstants::SourceNameAttributeName) ? InputSource.GetName() : Settings->OutputAttributeName;

		TUniquePtr<const IPCGAttributeAccessor> Accessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputData, InputSource);
		TUniquePtr<const IPCGAttributeAccessorKeys> Keys = PCGAttributeAccessorHelpers::CreateConstKeys(InputData, InputSource);

		if (!Accessor.IsValid() || !Keys.IsValid())
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeDoesNotExist", "Input attribute/property '{0}' does not exist on input {1}, skipped"), InputSource.GetDisplayText(), FText::AsNumber(i)));
			continue;
		}

		auto DoOperation = [&Accessor, &Keys, Operation = Settings->Operation, bMergeOutputAttributes = Settings->bMergeOutputAttributes, &OutputParams, &NewAttribute, OutputAttributeName](auto DummyValue) -> bool
		{
			using AttributeType = decltype(DummyValue);

			bool bSuccess = false;

			AttributeType OutputValue = PCG::Private::MetadataTraits<AttributeType>::ZeroValue();

			if (!OutputParams || !bMergeOutputAttributes)
			{
				OutputParams = NewObject<UPCGParamData>();
				NewAttribute = OutputParams->Metadata->CreateAttribute<AttributeType>(OutputAttributeName, OutputValue, /*bAllowInterpolation=*/ true, /*bOverrideParent=*/false);

				if (!NewAttribute)
				{
					OutputParams = nullptr;
					return false;
				}
			}

			FPCGMetadataAttribute<AttributeType>* TypedNewAttribute = static_cast<FPCGMetadataAttribute<AttributeType>*>(NewAttribute);
			check(TypedNewAttribute);

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
			case EPCGAttributeReduceOperation::Sum:
				bSuccess = PCGAttributeReduceElement::Sum<AttributeType>(*Keys, *Accessor, OutputValue);
				break;
			default:
				break;
			}

			if (bSuccess)
			{
				// Implementation note: since the default value does not match the value computed here
				// and because we might have multiple entries, we need to set it in the attribute
				TypedNewAttribute->SetValue(OutputParams->Metadata->AddEntry(), OutputValue);
			}

			return bSuccess;
		};

		if (!PCGMetadataAttribute::CallbackWithRightType(Accessor->GetUnderlyingType(), DoOperation))
		{
			PCGE_LOG(Error, GraphAndLog, FText::Format(LOCTEXT("AttributeOperationFailed", "Operation was not compatible with the attribute type {0} or could not create attribute '{1}' for input {2}"), PCG::Private::GetTypeNameText(Accessor->GetUnderlyingType()), FText::FromName(OutputAttributeName), FText::AsNumber(i)));
			continue;
		}

		if (ensure(OutputParams) && (Outputs.IsEmpty() || !Settings->bMergeOutputAttributes))
		{
			FPCGTaggedData& Output = Outputs.Add_GetRef(Inputs[i]);
			Output.Data = OutputParams;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
