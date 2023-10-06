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
	const FName FilterMinLabel = TEXT("FilterMin");
	const FName FilterMaxLabel = TEXT("FilterMax");
	const FName InFilterLabel = TEXT("InsideFilter");
	const FName OutFilterLabel = TEXT("OutsideFilter");

	constexpr int32 ChunkSize = 256;

#if WITH_EDITOR
	const FText FilterPinTooltip = LOCTEXT("FilterPinTooltip", "This pin accepts Statial data and Attribute Sets. If the data is Spatial, it will automatically sample input points in it. "
		"If it is points, it will sample if \"Spatial Query\" is enabled, otherwise points number need to match with input.");
#endif // WITH_EDITOR
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
				break;
			}
		}

		if constexpr (PCG::Private::MetadataTraits<T>::CanSearchString)
		{
			switch (Operation)
			{
			case EPCGPointFilterOperator::Substring:
				return PCG::Private::MetadataTraits<T>::Substring(Input1, Input2);
			case EPCGPointFilterOperator::Matches:
				return PCG::Private::MetadataTraits<T>::Matches(Input1, Input2);
			default:
				break;
			}
		}

		return false;
	}

	template <typename T>
	bool ApplyRange(const T& Input, const T& InMin, const T& InMax, bool bMinIncluded, bool bMaxIncluded)
	{
		if constexpr (PCG::Private::MetadataTraits<T>::CanCompare)
		{
			return (bMinIncluded ? PCG::Private::MetadataTraits<T>::GreaterOrEqual(Input, InMin) : PCG::Private::MetadataTraits<T>::Greater(Input, InMin)) &&
				(bMaxIncluded ? PCG::Private::MetadataTraits<T>::LessOrEqual(Input, InMax) : PCG::Private::MetadataTraits<T>::Less(Input, InMax));
		}
		else
		{
			return false;
		}
	}

	struct ThresholdInfo
	{
		TUniquePtr<const IPCGAttributeAccessor> ThresholdAccessor;
		TUniquePtr<const IPCGAttributeAccessorKeys> ThresholdKeys;
		bool bUseInputDataForThreshold = false;
		bool bUseSpatialQuery = false;

		UPCGPointData* ThresholdPointData = nullptr;
		const UPCGSpatialData* ThresholdSpatialData = nullptr;
	};

	bool InitialPrepareThresholdInfo(FPCGContext* InContext, TArray<FPCGTaggedData> FilterData, const FPCGPointFilterThresholdSettings& ThresholdSettings, ThresholdInfo& OutThresholdInfo)
	{
		if (ThresholdSettings.bUseConstantThreshold)
		{
			auto ConstantThreshold = [&OutThresholdInfo](auto&& Value)
			{
				using ConstantType = std::decay_t<decltype(Value)>;

				OutThresholdInfo.ThresholdAccessor = MakeUnique<FPCGConstantValueAccessor<ConstantType>>(std::forward<decltype(Value)>(Value));
				// Dummy keys
				OutThresholdInfo.ThresholdKeys = MakeUnique<FPCGAttributeAccessorKeysSingleObjectPtr<void>>();
			};

			ThresholdSettings.AttributeTypes.Dispatcher(ConstantThreshold);
		}
		else
		{
			if (!FilterData.IsEmpty())
			{
				const UPCGData* ThresholdData = FilterData[0].Data;

				if (const UPCGSpatialData* ThresholdSpatialData = Cast<const UPCGSpatialData>(ThresholdData))
				{
					// If the threshold is spatial or points (and spatial query is enabled), we'll use spatial query (meaning we'll have to sample points).
					// Don't create an accessor yet (ThresholdData = nullptr), it will be created further down.
					OutThresholdInfo.ThresholdSpatialData = ThresholdSpatialData;
					if (!ThresholdSpatialData->IsA<UPCGPointData>() || ThresholdSettings.bUseSpatialQuery)
					{
						OutThresholdInfo.bUseSpatialQuery = true;
						ThresholdData = nullptr;
					}
				}

				if (ThresholdData)
				{
					FPCGAttributePropertyInputSelector ThresholdSelector = ThresholdSettings.ThresholdAttribute.CopyAndFixLast(ThresholdData);
					OutThresholdInfo.ThresholdAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(ThresholdData, ThresholdSelector);
					OutThresholdInfo.ThresholdKeys = PCGAttributeAccessorHelpers::CreateConstKeys(ThresholdData, ThresholdSelector);
				}
			}
			else
			{
				OutThresholdInfo.bUseInputDataForThreshold = true;
			}
		}

		return true;
	}

	bool PrepareThresholdInfoFromInput(FPCGContext* InContext, const UPCGPointData* InputData, const FPCGPointFilterThresholdSettings& ThresholdSettings, ThresholdInfo& InOutThresholdInfo, int16 TargetType, bool bCheckCompare, bool bCheckStringSearch, const ThresholdInfo* OtherInfo = nullptr)
	{
		check(InContext && InputData);

		if (InOutThresholdInfo.bUseInputDataForThreshold)
		{
			// If we have no threshold accessor, we use the same data as input
			FPCGAttributePropertyInputSelector ThresholdSelector = ThresholdSettings.ThresholdAttribute.CopyAndFixLast(InputData);
			InOutThresholdInfo.ThresholdAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InputData, ThresholdSelector);
			InOutThresholdInfo.ThresholdKeys = PCGAttributeAccessorHelpers::CreateConstKeys(InputData, ThresholdSelector);
		}
		else if (InOutThresholdInfo.ThresholdSpatialData != nullptr && InOutThresholdInfo.bUseSpatialQuery)
		{
			// Don't do 2 spatial query if we are iterating on the same input
			if (OtherInfo != nullptr && OtherInfo->ThresholdSpatialData == InOutThresholdInfo.ThresholdSpatialData)
			{
				InOutThresholdInfo.ThresholdPointData = OtherInfo->ThresholdPointData;
			}
			else
			{
				// Reset the point data and reserving some points
				// No need to reserve the full number of points, since we'll go by chunk
				// Only allocate the chunk size
				InOutThresholdInfo.ThresholdPointData = NewObject<UPCGPointData>();
				InOutThresholdInfo.ThresholdPointData->InitializeFromData(InOutThresholdInfo.ThresholdSpatialData);
				InOutThresholdInfo.ThresholdPointData->GetMutablePoints().SetNum(PCGPointFilterConstants::ChunkSize);
			}

			// Accessor will be valid, but keys will point to default points. But since it is a view, it will be updated when we sample the points.
			FPCGAttributePropertyInputSelector ThresholdSelector = ThresholdSettings.ThresholdAttribute.CopyAndFixLast(InOutThresholdInfo.ThresholdPointData);
			InOutThresholdInfo.ThresholdAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(InOutThresholdInfo.ThresholdPointData, ThresholdSelector);
			InOutThresholdInfo.ThresholdKeys = PCGAttributeAccessorHelpers::CreateConstKeys(InOutThresholdInfo.ThresholdPointData, ThresholdSelector);
		}

		if (!InOutThresholdInfo.ThresholdAccessor.IsValid() || !InOutThresholdInfo.ThresholdKeys.IsValid())
		{
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(LOCTEXT("AttributeMissingForFilter", "DataToFilter does not have '{0}' threshold attribute/property"), FText::FromName(ThresholdSettings.ThresholdAttribute.GetName())));
			return false;
		}

		// Comparison between threshold and target data needs to be of the same type. So we have to make sure that we can
		// request target type from threshold type. ie. We need to make sure we can broadcast threshold type to target type.
		// For example: if target is double but threshold is int32, we can broadcast int32 to double, to compare a double with a double.
		if (!PCG::Private::IsBroadcastable(InOutThresholdInfo.ThresholdAccessor->GetUnderlyingType(), TargetType))
		{
			UEnum* PCGDataTypeEnum = StaticEnum<EPCGMetadataTypes>();
			FText ThresholdTypeName = FText::FromString(PCGDataTypeEnum ? PCGDataTypeEnum->GetNameStringByValue(InOutThresholdInfo.ThresholdAccessor->GetUnderlyingType()) : FString(TEXT("Unknown")));
			FText InputTypeName = FText::FromString(PCGDataTypeEnum ? PCGDataTypeEnum->GetNameStringByValue(TargetType) : FString(TEXT("Unknown")));
			PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(LOCTEXT("TypeConversionFailed", "Cannot use threshold type '{0}' on input target type '{1}'"),
				ThresholdTypeName,
				InputTypeName));
			return false;
		}

		// And also validate that types are comparable
		if (bCheckCompare)
		{
			bool bCanCompare = PCGMetadataAttribute::CallbackWithRightType(InOutThresholdInfo.ThresholdAccessor->GetUnderlyingType(), [](auto Dummy) -> bool
			{
				return PCG::Private::MetadataTraits<decltype(Dummy)>::CanCompare;
			});

			if (!bCanCompare)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, LOCTEXT("TypeComparisonFailed", "Cannot compare target type"));
				return false;
			}
		}

		if (bCheckStringSearch)
		{
			bool bCanSearchString = PCGMetadataAttribute::CallbackWithRightType(InOutThresholdInfo.ThresholdAccessor->GetUnderlyingType(), [](auto Dummy) -> bool
			{
				return PCG::Private::MetadataTraits<decltype(Dummy)>::CanSearchString;
			});

			if (!bCanSearchString)
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, LOCTEXT("TypeStringSearchFailed", "Cannot perform string operations on target type"));
				return false;
			}
		}

		// Check that if we have points as threshold, that the point data has the same number of point that the input data
		if (InOutThresholdInfo.ThresholdSpatialData != nullptr && !InOutThresholdInfo.bUseSpatialQuery)
		{
			if (InOutThresholdInfo.ThresholdKeys->GetNum() != InputData->GetPoints().Num())
			{
				PCGE_LOG_C(Warning, GraphAndLog, InContext, FText::Format(LOCTEXT("InvalidNumberOfThresholdPoints", "Threshold point data doesn't have the same number of points ({0}) than the input data ({1})."), InOutThresholdInfo.ThresholdKeys->GetNum(), InputData->GetPoints().Num()));
				return false;
			}
		}

		return true;
	}
}

TArray<FPCGPinProperties> UPCGPointFilterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPointFilterConstants::DataToFilterLabel, EPCGDataType::Point);

	if (!bUseConstantThreshold)
	{
		PinProperties.Emplace(PCGPointFilterConstants::FilterLabel, EPCGDataType::Any, /*bInAllowMultipleConnections=*/ false);
#if WITH_EDITOR
		PinProperties.Last().Tooltip = PCGPointFilterConstants::FilterPinTooltip;
#endif // WITH_EDITOR
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

FPCGElementPtr UPCGPointFilterRangeSettings::CreateElement() const
{
	return MakeShared<FPCGPointFilterRangeElement>();
}

UPCGPointFilterRangeSettings::UPCGPointFilterRangeSettings()
	: UPCGSettings()
{
	// Previous default object was: density for both selectors
	// Recreate the same default
	TargetAttribute.SetPointProperty(EPCGPointProperties::Density);
	MinThreshold.ThresholdAttribute.SetPointProperty(EPCGPointProperties::Density);
	MaxThreshold.ThresholdAttribute.SetPointProperty(EPCGPointProperties::Density);
}

TArray<FPCGPinProperties> UPCGPointFilterRangeSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPointFilterConstants::DataToFilterLabel, EPCGDataType::Point);

	if (!MinThreshold.bUseConstantThreshold)
	{
		PinProperties.Emplace(PCGPointFilterConstants::FilterMinLabel, EPCGDataType::Any, /*bInAllowMultipleConnections=*/ false);
#if WITH_EDITOR
		PinProperties.Last().Tooltip = PCGPointFilterConstants::FilterPinTooltip;
#endif // WITH_EDITOR
	}

	if (!MaxThreshold.bUseConstantThreshold)
	{
		PinProperties.Emplace(PCGPointFilterConstants::FilterMaxLabel, EPCGDataType::Any, /*bInAllowMultipleConnections=*/ false);
#if WITH_EDITOR
		PinProperties.Last().Tooltip = PCGPointFilterConstants::FilterPinTooltip;
#endif // WITH_EDITOR
	}

	return PinProperties;
}

TArray<FPCGPinProperties> UPCGPointFilterRangeSettings::OutputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPointFilterConstants::InFilterLabel, EPCGDataType::Point);
	PinProperties.Emplace(PCGPointFilterConstants::OutFilterLabel, EPCGDataType::Point);

	return PinProperties;
}

bool FPCGPointFilterElementBase::DoFiltering(FPCGContext* Context, EPCGPointFilterOperator InOperation, const FPCGAttributePropertyInputSelector& InTargetAttribute, const FPCGPointFilterThresholdSettings& FirstThreshold, const FPCGPointFilterThresholdSettings* SecondThreshold) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointFilterElementBase::DoFiltering);
	check(Context);

	TArray<FPCGTaggedData> DataToFilter = Context->InputData.GetInputsByPin(PCGPointFilterConstants::DataToFilterLabel);
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	TArray<FPCGTaggedData> FirstFilterData = SecondThreshold ? Context->InputData.GetInputsByPin(PCGPointFilterConstants::FilterMinLabel) : Context->InputData.GetInputsByPin(PCGPointFilterConstants::FilterLabel);
	TArray<FPCGTaggedData> SecondFilterData = SecondThreshold ? Context->InputData.GetInputsByPin(PCGPointFilterConstants::FilterMaxLabel) : TArray<FPCGTaggedData>{};

	const EPCGPointFilterOperator Operator = InOperation;

	// If there is no input, do nothing
	if (DataToFilter.IsEmpty())
	{
		return true;
	}

	// Only support second threshold with the InRange Operation.
	// We can't have a second threshold if it is not InRange
	if (!ensure((SecondThreshold != nullptr) == (InOperation == EPCGPointFilterOperator::InRange)))
	{
		return true;
	}

	PCGPointFilterHelpers::ThresholdInfo FirstThresholdInfo;
	PCGPointFilterHelpers::ThresholdInfo SecondThresholdInfo;

	if (!PCGPointFilterHelpers::InitialPrepareThresholdInfo(Context, FirstFilterData, FirstThreshold, FirstThresholdInfo))
	{
		return true;
	}

	if (SecondThreshold && !PCGPointFilterHelpers::InitialPrepareThresholdInfo(Context, SecondFilterData, *SecondThreshold, SecondThresholdInfo))
	{
		return true;
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

		FPCGAttributePropertyInputSelector TargetAttribute = InTargetAttribute.CopyAndFixLast(OriginalData);
		TUniquePtr<const IPCGAttributeAccessor> TargetAccessor = PCGAttributeAccessorHelpers::CreateConstAccessor(OriginalData, TargetAttribute);
		TUniquePtr<const IPCGAttributeAccessorKeys> TargetKeys = PCGAttributeAccessorHelpers::CreateConstKeys(OriginalData, TargetAttribute);

		if (!TargetAccessor.IsValid() || !TargetKeys.IsValid())
		{
			PCGE_LOG(Warning, GraphAndLog, FText::Format(LOCTEXT("TargetMissingAttribute", "TargetData doesn't have target attribute/property '{0}'"), FText::FromName(TargetAttribute.GetName())));
			ForwardInputToOutFilterPin();
			continue;
		}

		const int16 TargetType = TargetAccessor->GetUnderlyingType();
		const bool bCheckStringSearch = (Operator == EPCGPointFilterOperator::Substring || Operator == EPCGPointFilterOperator::Matches);
		const bool bCheckCompare = (Operator != EPCGPointFilterOperator::Equal) && (Operator != EPCGPointFilterOperator::NotEqual) && !bCheckStringSearch;

		if (!PCGPointFilterHelpers::PrepareThresholdInfoFromInput(Context, OriginalData, FirstThreshold, FirstThresholdInfo, TargetType, bCheckCompare, bCheckStringSearch))
		{
			ForwardInputToInFilterPin();
			continue;
		}

		if (SecondThreshold && !PCGPointFilterHelpers::PrepareThresholdInfoFromInput(Context, OriginalData, *SecondThreshold, SecondThresholdInfo, TargetType, bCheckCompare, bCheckStringSearch, &FirstThresholdInfo))
		{
			ForwardInputToInFilterPin();
			continue;
		}

		if (!PCG::Private::IsBroadcastable(FirstThresholdInfo.ThresholdAccessor->GetUnderlyingType(), TargetAccessor->GetUnderlyingType())
			|| (SecondThresholdInfo.ThresholdAccessor.IsValid() && !PCG::Private::IsBroadcastable(SecondThresholdInfo.ThresholdAccessor->GetUnderlyingType(), TargetAccessor->GetUnderlyingType())))
		{
			PCGE_LOG(Warning, GraphAndLog, LOCTEXT("TypeCannotBeConverted", "Cannot convert threshold type to target type"));
			ForwardInputToInFilterPin();
			continue;
		}

		UPCGPointData* InFilterData = NewObject<UPCGPointData>();
		UPCGPointData* OutFilterData = NewObject<UPCGPointData>();

		InFilterData->InitializeFromData(OriginalData);
		OutFilterData->InitializeFromData(OriginalData);
		TArray<FPCGPoint>& InFilterPoints = InFilterData->GetMutablePoints();
		TArray<FPCGPoint>& OutFilterPoints = OutFilterData->GetMutablePoints();

		InFilterPoints.Reserve(OriginalPoints.Num());
		OutFilterPoints.Reserve(OriginalPoints.Num());

		auto Operation = [&InFilterPoints, &OutFilterPoints, &Operator, &FirstThresholdInfo, &SecondThresholdInfo, &TargetAccessor, &TargetKeys, &OriginalPoints, &FirstThreshold, &SecondThreshold](auto Dummy) -> bool
		{
			using Type = decltype(Dummy);

			const int32 NumberOfEntries = TargetKeys->GetNum();

			if (NumberOfEntries <= 0)
			{
				return false;
			}

			TArray<Type, TInlineAllocator<PCGPointFilterConstants::ChunkSize>> TargetValues;
			TArray<Type, TInlineAllocator<PCGPointFilterConstants::ChunkSize>> FirstThresholdValues;
			TArray<Type, TInlineAllocator<PCGPointFilterConstants::ChunkSize>> SecondThresholdValues;
			TArray<bool, TInlineAllocator<PCGPointFilterConstants::ChunkSize>> SkipTests;
			TargetValues.SetNum(PCGPointFilterConstants::ChunkSize);
			FirstThresholdValues.SetNum(PCGPointFilterConstants::ChunkSize);
			SecondThresholdValues.SetNum(PCGPointFilterConstants::ChunkSize);
			SkipTests.SetNumZeroed(PCGPointFilterConstants::ChunkSize); // All initialized to false

			const int32 NumberOfIterations = (NumberOfEntries + PCGPointFilterConstants::ChunkSize - 1) / PCGPointFilterConstants::ChunkSize;

			for (int32 i = 0; i < NumberOfIterations; ++i)
			{
				const int32 StartIndex = i * PCGPointFilterConstants::ChunkSize;
				const int32 Range = FMath::Min(NumberOfEntries - StartIndex, PCGPointFilterConstants::ChunkSize);
				TArrayView<Type> TargetView(TargetValues.GetData(), Range);
				TArrayView<Type> FirstThresholdView(FirstThresholdValues.GetData(), Range);
				TArrayView<Type> SecondThresholdView(SecondThresholdValues.GetData(), Range);

				// Sampling the points if needed
				auto SamplePointData = [Range, StartIndex, &OriginalPoints, &SkipTests](UPCGPointData* InPointData, const UPCGSpatialData* InSpatialData)
				{
					if (InPointData != nullptr)
					{
						// Threshold points only have "ChunkSize" points.
						TArray<FPCGPoint>& ThresholdPoints = InPointData->GetMutablePoints();
						for (int32 j = 0; j < Range; ++j)
						{
							FPCGPoint ThresholdPoint;
							const FPCGPoint& SourcePoint = OriginalPoints[StartIndex + j];
							// If we already mark this point to skip test, don't even try to sample.
							// If the sample fails, mark the point to skip test.
							if (!SkipTests[j] && InSpatialData->SamplePoint(SourcePoint.Transform, SourcePoint.GetLocalBounds(), ThresholdPoint, InPointData->Metadata))
							{
								ThresholdPoints[j] = ThresholdPoint;
							}
							else
							{
								SkipTests[j] = true;
							}
						}
					}
				};

				SamplePointData(FirstThresholdInfo.ThresholdPointData, FirstThresholdInfo.ThresholdSpatialData);

				if (FirstThresholdInfo.ThresholdPointData != SecondThresholdInfo.ThresholdPointData)
				{
					SamplePointData(SecondThresholdInfo.ThresholdPointData, SecondThresholdInfo.ThresholdSpatialData);
				}

				// If ThresholdView point on ThresholdPointData points, there are only "ChunkSize" points in it.
				// But it wraps around, and since StartIndex is a multiple of "ChunkSize", we'll always start at point 0, as wanted. 
				if (!TargetAccessor->GetRange(TargetView, StartIndex, *TargetKeys) ||
					!FirstThresholdInfo.ThresholdAccessor->GetRange(FirstThresholdView, StartIndex, *FirstThresholdInfo.ThresholdKeys, EPCGAttributeAccessorFlags::AllowBroadcast) ||
					(SecondThresholdInfo.ThresholdAccessor.IsValid() && !SecondThresholdInfo.ThresholdAccessor->GetRange(SecondThresholdView, StartIndex, *SecondThresholdInfo.ThresholdKeys, EPCGAttributeAccessorFlags::AllowBroadcast)))
				{
					return false;
				}

				for (int32 j = 0; j < Range; ++j)
				{
					if (SkipTests[j])
					{
						InFilterPoints.Add(OriginalPoints[StartIndex + j]);
						continue;
					}

					const bool bShouldKeep = (Operator == EPCGPointFilterOperator::InRange ? 
						PCGPointFilterHelpers::ApplyRange(TargetValues[j], FirstThresholdValues[j], SecondThresholdValues[j], FirstThreshold.bInclusive, SecondThreshold->bInclusive) : 
						PCGPointFilterHelpers::ApplyCompare(TargetValues[j], FirstThresholdValues[j], Operator));

					if (bShouldKeep)
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

bool FPCGPointFilterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointFilterElement::Execute);

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

	FPCGPointFilterThresholdSettings ThresholdSettings{};
	ThresholdSettings.bUseConstantThreshold = Settings->bUseConstantThreshold;
	ThresholdSettings.bUseSpatialQuery = Settings->bUseSpatialQuery;
	ThresholdSettings.ThresholdAttribute = Settings->ThresholdAttribute;
	ThresholdSettings.AttributeTypes = Settings->AttributeTypes;

	return DoFiltering(Context, Settings->Operator, Settings->TargetAttribute, ThresholdSettings);
}

bool FPCGPointFilterRangeElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointFilterRangeElement::Execute);

#if !WITH_EDITOR
	const bool bHasInFilterOutputPin = Context->Node && Context->Node->IsOutputPinConnected(PCGPointFilterConstants::InFilterLabel);
	const bool bHasOutsideFilterOutputPin = Context->Node && Context->Node->IsOutputPinConnected(PCGPointFilterConstants::OutFilterLabel);

	// Early out - only in non-editor builds, otherwise we will potentially poison the cache, since it is input-driven
	if (!bHasInFilterOutputPin && !bHasOutsideFilterOutputPin)
	{
		return true;
	}
#endif

	const UPCGPointFilterRangeSettings* Settings = Context->GetInputSettings<UPCGPointFilterRangeSettings>();
	check(Settings);

	return DoFiltering(Context, EPCGPointFilterOperator::InRange, Settings->TargetAttribute, Settings->MinThreshold, &Settings->MaxThreshold);
}

#undef LOCTEXT_NAMESPACE
