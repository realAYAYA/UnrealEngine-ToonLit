// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPointFilter.h"

#include "PCGContext.h"
#include "PCGCustomVersion.h"
#include "PCGParamData.h"
#include "PCGPin.h"
#include "Data/PCGPointData.h"
#include "Data/PCGSpatialData.h"
#include "Metadata/Accessors/IPCGAttributeAccessorTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGCustomAccessor.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(PCGPointFilter)

#define LOCTEXT_NAMESPACE "PCGPointFilterElement"

namespace PCGPointFilterConstants
{
	const FName DataToFilterLabel = TEXT("In");
	const FName FilterLabel = TEXT("Filter");
	const FName InFilterLabel = TEXT("InsideFilter");
	const FName OutFilterLabel = TEXT("OutsideFilter");

	constexpr int32 ChunkSize = 256;
}

namespace PCGPointFilterHelpers
{
	template <typename T>
	bool ApplyCompare(const T& Input1, const T& Input2, EPCGPointFilterOperator Operation)
	{
		if (Operation == EPCGPointFilterOperator::Equal)
		{
			return PCG::Private::MetadataTraits<T>::Equal(Input1, Input2);
		}
		else if (Operation == EPCGPointFilterOperator::NotEqual)
		{
			return !PCG::Private::MetadataTraits<T>::Equal(Input1, Input2);
		}

		if constexpr (PCG::Private::MetadataTraits<T>::CanCompare)
		{
			switch (Operation)
			{
			case EPCGPointFilterOperator::Greater:
				return PCG::Private::MetadataTraits<T>::Greater(Input1, Input2);
			case EPCGPointFilterOperator::GreaterOrEqual:
				return PCG::Private::MetadataTraits<T>::GreaterOrEqual(Input1, Input2);
			case EPCGPointFilterOperator::Lesser:
				return PCG::Private::MetadataTraits<T>::Less(Input1, Input2);
			case EPCGPointFilterOperator::LesserOrEqual:
				return PCG::Private::MetadataTraits<T>::LessOrEqual(Input1, Input2);
			default:
				return false;
			}
		}
		else
		{
			return false;
		}
	}
}

TArray<FPCGPinProperties> UPCGPointFilterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPointFilterConstants::DataToFilterLabel, EPCGDataType::Point);

	if (!bUseConstantThreshold)
	{
		PinProperties.Emplace(PCGPointFilterConstants::FilterLabel, EPCGDataType::Any, /*bInAllowMultipleConnections=*/ false);
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGPointFilterSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPointFilterConstants::InFilterLabel, EPCGDataType::Point);
	PinProperties.Emplace(PCGPointFilterConstants::OutFilterLabel, EPCGDataType::Point);

	return PinProperties;
}

FPCGElementPtr UPCGPointFilterSettings::CreateElement() const
{
	return MakeShared<FPCGPointFilterElement>();
}

UPCGPointFilterSettings::UPCGPointFilterSettings()
	: UPCGSettings()
{
	// Previous default object was: density for both selectors
	// Recreate the same default
	TargetAttribute.SetPointProperty(EPCGPointProperties::Density);
	ThresholdAttribute.SetPointProperty(EPCGPointProperties::Density);
}

void UPCGPointFilterSettings::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
PRAGMA_DISABLE_DEPRECATION_WARNINGS
	if ((TargetFilterType_DEPRECATED != EPCGPointTargetFilterType::Property)
		|| (TargetPointProperty_DEPRECATED != EPCGPointProperties::Density)
		|| (TargetAttributeName_DEPRECATED != NAME_None)
		|| (ThresholdFilterType_DEPRECATED != EPCGPointThresholdType::Property)
		|| (ThresholdPointProperty_DEPRECATED != EPCGPointProperties::Density)
		|| (ThresholdAttributeName_DEPRECATED != NAME_None))
	{
		if (TargetFilterType_DEPRECATED == EPCGPointTargetFilterType::Property)
		{
			TargetAttribute.SetPointProperty(TargetPointProperty_DEPRECATED);
		}
		else
		{
			TargetAttribute.SetAttributeName(TargetAttributeName_DEPRECATED);
		}

		if (ThresholdFilterType_DEPRECATED == EPCGPointThresholdType::Property)
		{
			ThresholdAttribute.SetPointProperty(ThresholdPointProperty_DEPRECATED);
		}
		else if (ThresholdFilterType_DEPRECATED == EPCGPointThresholdType::Metadata)
		{
			ThresholdAttribute.SetAttributeName(ThresholdAttributeName_DEPRECATED);
		}
		else
		{
			bUseConstantThreshold = true;
		}
	}

	if (bUseConstantThreshold && 
		((ThresholdConstantType_DEPRECATED != EPCGPointFilterConstantType::Float)
		|| (FloatConstant_DEPRECATED != 0.0f)))
	{
		switch (ThresholdConstantType_DEPRECATED)
		{
		case EPCGPointFilterConstantType::Float:
			AttributeTypes.Type = EPCGMetadataTypes::Float;
			AttributeTypes.FloatValue = FloatConstant_DEPRECATED;
			break;
		case EPCGPointFilterConstantType::Integer64:
			AttributeTypes.Type = EPCGMetadataTypes::Integer64;
			AttributeTypes.IntValue = Integer64Constant_DEPRECATED;
			break;
		case EPCGPointFilterConstantType::Vector:
			AttributeTypes.Type = EPCGMetadataTypes::Vector;
			AttributeTypes.VectorValue = VectorConstant_DEPRECATED;
			break;
		case EPCGPointFilterConstantType::Vector4:
			AttributeTypes.Type = EPCGMetadataTypes::Vector4;
			AttributeTypes.Vector4Value = Vector4Constant_DEPRECATED;
			break;
		case EPCGPointFilterConstantType::String:
			AttributeTypes.Type = EPCGMetadataTypes::String;
			AttributeTypes.StringValue = StringConstant_DEPRECATED;
			break;
		}
	}

	// Default values
	TargetFilterType_DEPRECATED = EPCGPointTargetFilterType::Property;
	TargetPointProperty_DEPRECATED = EPCGPointProperties::Density;
	TargetAttributeName_DEPRECATED = NAME_None;
	ThresholdFilterType_DEPRECATED = EPCGPointThresholdType::Property;
	ThresholdPointProperty_DEPRECATED = EPCGPointProperties::Density;
	ThresholdAttributeName_DEPRECATED = NAME_None;
	ThresholdConstantType_DEPRECATED = EPCGPointFilterConstantType::Float;
	FloatConstant_DEPRECATED = 0.0f;
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITOR
}

bool FPCGPointFilterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointFilterElement::Execute);
	check(Context);

#if !WITH_EDITOR
	const bool bHasInFilterOutputPin = Context->Node && Context->Node->IsOutputPinConnected(PCGPointFilterConstants::InFilterLabel);
	const bool bHasOutsideFilterOutputPin = Context->Node && Context->Node->IsOutputPinConnected(PCGPointFilterConstants::OutFilterLabel);

	// Early out - only in non-editor builds, otherwise we will potentially poison the cache, since it is input-driven
	if (!bHasInFilterOutputPin && !bHasOutsideFilterOutputPin)
	{
		return true;
	}
#endif

	const UPCGPointFilterSettings* Settings = Context->GetInputSettings<UPCGPointFilterSettings>();
	check(Settings);

	TArray<FPCGTaggedData> DataToFilter = Context->InputData.GetInputsByPin(PCGPointFilterConstants::DataToFilterLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	TArray<FPCGTaggedData> FilterData = Context->InputData.GetInputsByPin(PCGPointFilterConstants::FilterLabel);

	const EPCGPointFilterOperator Operator = Settings->Operator;
	const bool bUseConstantThreshold = Settings->bUseConstantThreshold;
	// If the filter is a param data, disable bUseSpatialQuery. Therefore make it non-const, to be modified below.
	bool bUseSpatialQuery = Settings->bUseSpatialQuery;
	// TODO: allow selector overrides for ThresholdAttribute and TargetAttribute

	// If there is no input, do nothing
	if (DataToFilter.IsEmpty())
	{
		return true;
	}

	TUniquePtr<const IPCGAttributeAccessor> ThresholdAccessor;
	TUniquePtr<const IPCGAttributeAccessorKeys> ThresholdKeys;
	bool bUseInputDataForThreshold = false;

	UPCGPointData* ThresholdPointData = nullptr;
	const UPCGSpatialData* ThresholdSpatialData = nullptr;

	if (bUseConstantThreshold)
	{
		auto ConstantThreshold = [&ThresholdAccessor, &ThresholdKeys](auto&& Value)
		{
			using ConstantType = std::decay_t<decltype(Value)>;

			ThresholdAccessor = MakeUnique<FPCGConstantValueAccessor<ConstantType>>(std::forward<decltype(Value)>(Value));
			// Dummy keys
			ThresholdKeys = MakeUnique<FPCGAttributeAccessorKeysSingleObjectPtr<void>>();
		};

		Settings->AttributeTypes.Dispatcher(ConstantThreshold);
	}
	else
	{
		if (!FilterData.IsEmpty())
		{
			const UPCGData* ThresholdData = FilterData[0].Data;

			if (const UPCGSpatialData* TentativeThresholdSpatialData = Cast<const UPCGSpatialData>(ThresholdData))
			{
				// If the threshold is spatial, and we use spatial query, it means we'll have to sample points.
				// Don't create an accessor yet (ThresholdData = nullptr), it will be created further down.
				// Otherwise, we convert it to point data and create the accessor.
				if (bUseSpatialQuery)
				{
					ThresholdSpatialData = TentativeThresholdSpatialData;
					ThresholdData = nullptr;
				}
				else
				{
					ThresholdSpatialData = TentativeThresholdSpatialData->ToPointData();
					if (!ThresholdSpatialData)
					{
						PCGE_LOG(Error, GraphAndLog, LOCTEXT("CannotGetPointData", "Unable to get Point data from filter input"));
						return true;
					}

					ThresholdData = ThresholdSpatialData;
				}
			}
			else if (ThresholdData->IsA<UPCGParamData>())
			{
				// Disable spatial query as it doesn't make sense
				bUseSpatialQuery = false;
			}

			if (ThresholdData)
			{
				ThresholdAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(ThresholdData, Settings->ThresholdAttribute);
				ThresholdKeys = PCGAttributeAccessorHelpers::CreateConstKeys(ThresholdData, Settings->ThresholdAttribute);
			}
		}
		else
		{
			bUseInputDataForThreshold = true;
		}
	}

	for (const FPCGTaggedData& Input : DataToFilter)
	{
		const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Input.Data);
		if (!SpatialInput)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("MoveNonSpatialData", "Please move non-Spatial data to other pins"));
			continue;
		}

		const UPCGPointData* OriginalData = SpatialInput->ToPointData(Context);
		if (!OriginalData)
		{
			PCGE_LOG(Error, GraphAndLog, LOCTEXT("NoPointDataInInput", "Unable to get point data from input"));
			continue;
		}

		// Helper lambdas to fail nicely and forward input to in/out filter pin
		// If there is a problem with threshold -> forward to InFilter
		auto ForwardInputToInFilterPin = [&Outputs, Input]()
		{
			FPCGTaggedData& InFilterOutput = Outputs.Add_GetRef(Input);
			InFilterOutput.Pin = PCGPointFilterConstants::InFilterLabel;
		};

		// If there is a problem with target -> forward to OutFilter
		auto ForwardInputToOutFilterPin = [&Outputs, Input]()
		{
			FPCGTaggedData& OutFilterOutput = Outputs.Add_GetRef(Input);
			OutFilterOutput.Pin = PCGPointFilterConstants::OutFilterLabel;
		};

		const TArray<FPCGPoint>& OriginalPoints = OriginalData->GetPoints();

		TUniquePtr<const IPCGAttributeAccessor> TargetAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(OriginalData, Settings->TargetAttribute);
		TUniquePtr<const IPCGAttributeAccessorKeys> TargetKeys = PCGAttributeAccessorHelpers::CreateConstKeys(OriginalData, Settings->TargetAttribute);

		if (!TargetAccessor.IsValid() || !TargetKeys.IsValid())
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("TargetMissingAttribute", "TargetData doesn't have target attribute/property '{0}'"), FText::FromName(Settings->TargetAttribute.GetName())));
			ForwardInputToOutFilterPin();
			continue;
		}

		if (bUseInputDataForThreshold)
		{
			// If we have no threshold accessor, we use the same data as input
			ThresholdAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(OriginalData, Settings->ThresholdAttribute);
			ThresholdKeys = PCGAttributeAccessorHelpers::CreateConstKeys(OriginalData, Settings->ThresholdAttribute);
		}
		else if (ThresholdSpatialData != nullptr && bUseSpatialQuery)
		{
			// Reset the point data and reserving some points
			// No need to reserve the full number of points, since we'll go by chunk
			// Only allocate the chunk size
			ThresholdPointData = NewObject<UPCGPointData>();
			ThresholdPointData->InitializeFromData(ThresholdSpatialData);
			ThresholdPointData->GetMutablePoints().SetNum(PCGPointFilterConstants::ChunkSize);

			// Accessor will be valid, but keys will point to default points. But since it is a view, it will be updated when we sample the points.
			ThresholdAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(ThresholdPointData, Settings->ThresholdAttribute);
			ThresholdKeys = PCGAttributeAccessorHelpers::CreateConstKeys(ThresholdPointData, Settings->ThresholdAttribute);
		}

		if (!ThresholdAccessor.IsValid() || !ThresholdKeys.IsValid())
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("AttributeMissingForFilter", "DataToFilter does not have '{0}' threshold attribute/property"), FText::FromName(Settings->ThresholdAttribute.GetName())));
			ForwardInputToInFilterPin();
			continue;
		}

		// Comparison between threshold and target data needs to be of the same type. So we have to make sure that we can
		// request target type from threshold type. ie. We need to make sure we can broadcast threshold type to target type.
		// For example: if target is double but threshold is int32, we can broadcast int32 to double, to compare a double with a double.
		if (!PCG::Private::IsBroadcastable(ThresholdAccessor->GetUnderlyingType(), TargetAccessor->GetUnderlyingType()))
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("TypeConversionFailed", "Cannot broadcast threshold type to target type"));
			ForwardInputToInFilterPin();
			continue;
		}

		// And also validate that types are comparable
		if (Operator != EPCGPointFilterOperator::Equal && Operator != EPCGPointFilterOperator::NotEqual)
		{
			bool bCanCompare = PCGMetadataAttribute::CallbackWithRightType(TargetAccessor->GetUnderlyingType(), [](auto Dummy) -> bool
			{
				return PCG::Private::MetadataTraits<decltype(Dummy)>::CanCompare;
			});

			if (!bCanCompare)
			{
				PCGE_LOG(Warning, GraphAndLog, LOCTEXT("TypeComparisonFailed", "Cannot compare target type"));
				ForwardInputToOutFilterPin();
				continue;
			}
		}

		UPCGPointData* InFilterData = NewObject<UPCGPointData>();
		UPCGPointData* OutFilterData = NewObject<UPCGPointData>();

		InFilterData->InitializeFromData(OriginalData);
		OutFilterData->InitializeFromData(OriginalData);
		TArray<FPCGPoint>& InFilterPoints = InFilterData->GetMutablePoints();
		TArray<FPCGPoint>& OutFilterPoints = OutFilterData->GetMutablePoints();

		InFilterPoints.Reserve(OriginalPoints.Num());
		OutFilterPoints.Reserve(OriginalPoints.Num());

		auto Operation = [&InFilterPoints, &OutFilterPoints, &Operator, &ThresholdAccessor, &ThresholdKeys, &TargetAccessor, &TargetKeys, &OriginalPoints, ThresholdPointData, ThresholdSpatialData](auto Dummy) -> bool
		{
			using Type = decltype(Dummy);

			const int32 NumberOfEntries = TargetKeys->GetNum();

			if (NumberOfEntries <= 0)
			{
				return false;
			}

			TArray<Type, TInlineAllocator<PCGPointFilterConstants::ChunkSize>> TargetValues;
			TArray<Type, TInlineAllocator<PCGPointFilterConstants::ChunkSize>> ThresholdValues;
			TArray<bool, TInlineAllocator<PCGPointFilterConstants::ChunkSize>> SkipTests;
			TargetValues.SetNum(PCGPointFilterConstants::ChunkSize);
			ThresholdValues.SetNum(PCGPointFilterConstants::ChunkSize);
			SkipTests.SetNum(PCGPointFilterConstants::ChunkSize);

			const int32 NumberOfIterations = (NumberOfEntries + PCGPointFilterConstants::ChunkSize - 1) / PCGPointFilterConstants::ChunkSize;

			for (int32 i = 0; i < NumberOfIterations; ++i)
			{
				const int32 StartIndex = i * PCGPointFilterConstants::ChunkSize;
				const int32 Range = FMath::Min(NumberOfEntries - StartIndex, PCGPointFilterConstants::ChunkSize);
				TArrayView<Type> TargetView(TargetValues.GetData(), Range);
				TArrayView<Type> ThresholdView(ThresholdValues.GetData(), Range);

				// Sampling the points if needed
				if (ThresholdPointData != nullptr)
				{
					// Threshold points only have "ChunkSize" points.
					TArray<FPCGPoint>& ThresholdPoints = ThresholdPointData->GetMutablePoints();
					for (int32 j = 0; j < Range; ++j)
					{
						FPCGPoint ThresholdPoint;
						const FPCGPoint& SourcePoint = OriginalPoints[StartIndex + j];
						if (ThresholdSpatialData->SamplePoint(SourcePoint.Transform, SourcePoint.GetLocalBounds(), ThresholdPoint, ThresholdPointData->Metadata))
						{
							ThresholdPoints[j] = ThresholdPoint;
							SkipTests[j] = false;
						}
						else
						{
							SkipTests[j] = true;
						}
					}
				}

				// If ThresholdView point on ThresholdPointData points, there are only "ChunkSize" points in it.
				// But it wraps around, and since StartIndex is a multiple of "ChunkSize", we'll always start at point 0, as wanted. 
				if (!TargetAccessor->GetRange(TargetView, StartIndex, *TargetKeys) ||
					!ThresholdAccessor->GetRange(ThresholdView, StartIndex, *ThresholdKeys, EPCGAttributeAccessorFlags::AllowBroadcast))
				{
					return false;
				}

				for (int32 j = 0; j < Range; ++j)
				{
					if (SkipTests[j] || PCGPointFilterHelpers::ApplyCompare(TargetValues[j], ThresholdValues[j], Operator))
					{
						InFilterPoints.Add(OriginalPoints[StartIndex + j]);
					}
					else
					{
						OutFilterPoints.Add(OriginalPoints[StartIndex + j]);
					}
				}
			}

			return true;
		};

		if (PCGMetadataAttribute::CallbackWithRightType(TargetAccessor->GetUnderlyingType(), Operation))
		{
			FPCGTaggedData& InFilterOutput = Outputs.Add_GetRef(Input);
			InFilterOutput.Pin = PCGPointFilterConstants::InFilterLabel;
			InFilterOutput.Data = InFilterData;
			InFilterOutput.Tags = Input.Tags;

			FPCGTaggedData& OutFilterOutput = Outputs.Add_GetRef(Input);
			OutFilterOutput.Pin = PCGPointFilterConstants::OutFilterLabel;
			OutFilterOutput.Data = OutFilterData;
			OutFilterOutput.Tags = Input.Tags;
		}
	}

	return true;
}

#undef LOCTEXT_NAMESPACE
