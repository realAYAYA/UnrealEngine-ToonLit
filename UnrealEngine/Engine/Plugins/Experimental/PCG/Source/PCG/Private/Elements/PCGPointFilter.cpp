// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGPointFilter.h"
#include "Data/PCGSpatialData.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Helpers/PCGAsync.h"

namespace PCGPointFilterConstants
{
	const FName DataToFilterLabel = TEXT("In");
	const FName FilterLabel = TEXT("Filter");
	const FName InFilterLabel = TEXT("InsideFilter");
	const FName OutFilterLabel = TEXT("OutsideFilter");
}

namespace PCGPointFilterHelpers
{
	TFunction<void(const FPCGPoint&, const UPCGMetadata*, void*)> ConstructPropertyGetter(EPCGPointProperties TargetPointProperty, EPCGPointFilterConstantType& OutType)
	{
		if (TargetPointProperty == EPCGPointProperties::Density)
		{
			OutType = EPCGPointFilterConstantType::Float;
			return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
			{
				*(static_cast<float*>(OutData)) = InPoint.Density;
			};
		}
		else if (TargetPointProperty == EPCGPointProperties::BoundsMin)
		{
			OutType = EPCGPointFilterConstantType::Vector;
			return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
			{
				*(static_cast<FVector*>(OutData)) = InPoint.BoundsMin;
			};
		}
		else if (TargetPointProperty == EPCGPointProperties::BoundsMax)
		{
			OutType = EPCGPointFilterConstantType::Vector;
			return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
			{
				*(static_cast<FVector*>(OutData)) = InPoint.BoundsMax;
			};
		}
		else if (TargetPointProperty == EPCGPointProperties::Extents)
		{
			OutType = EPCGPointFilterConstantType::Vector;
			return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
			{
				*(static_cast<FVector*>(OutData)) = InPoint.GetExtents();
			};
		}
		else if (TargetPointProperty == EPCGPointProperties::Color)
		{
			OutType = EPCGPointFilterConstantType::Vector4;
			return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
			{
				*(static_cast<FVector4*>(OutData)) = InPoint.Color;
			};
		}
		else if (TargetPointProperty == EPCGPointProperties::Position)
		{
			OutType = EPCGPointFilterConstantType::Vector;
			return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
			{
				*(static_cast<FVector*>(OutData)) = InPoint.Transform.GetLocation();
			};
		}
		//else if (TargetPointProperty == EPCGPointProperties::Rotation)
		//{
		//	OutType = EPCGPointFilterConstantType::Rotation;
		//	return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
		//	{
		//		*(static_cast<FQuat*>(OutData)) = InPoint.Transform.GetRotation();
		//	};
		//}
		else if (TargetPointProperty == EPCGPointProperties::Scale)
		{
			OutType = EPCGPointFilterConstantType::Vector;
			return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
			{
				*(static_cast<FVector*>(OutData)) = InPoint.Transform.GetScale3D();
			};
		}
		//else if (TargetPointProperty == EPCGPointProperties::Transform)
		//{
		//	OutType = EPCGPointFilterConstantType::Transform;
		//	return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
		//	{
		//		*(static_cast<FTransform*>(OutData)) = InPoint.Transform;
		//	};
		//}
		else if (TargetPointProperty == EPCGPointProperties::Steepness)
		{
			OutType = EPCGPointFilterConstantType::Float;
			return [](const FPCGPoint& InPoint, const UPCGMetadata*, void* OutData)
			{
				*(static_cast<float*>(OutData)) = InPoint.Steepness;
			};
		}
		else
		{
			// Log error
			OutType = EPCGPointFilterConstantType::Unknown;
			return [](const FPCGPoint&, const UPCGMetadata*, void*) {};
		}
	}

	TFunction<void(const FPCGPoint&, const UPCGMetadata*, void*)> ConstructAttributeGetter(
		const UPCGMetadata* Metadata, 
		const FName& AttributeName, 
		EPCGPointFilterConstantType& OutType)
	{
		OutType = EPCGPointFilterConstantType::Unknown;

		const FPCGMetadataAttributeBase* AttributeBase = Metadata->GetConstAttribute(AttributeName);
		if (AttributeBase)
		{
			if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<bool>::Id)
			{
				OutType = EPCGPointFilterConstantType::Integer64;
				return [AttributeBase](const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, void* OutData)
				{
					*(static_cast<int64*>(OutData)) = static_cast<const FPCGMetadataAttribute<bool>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
				};
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<int32>::Id)
			{
				OutType = EPCGPointFilterConstantType::Integer64;
				return [AttributeBase](const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, void* OutData)
				{
					*(static_cast<int64*>(OutData)) = static_cast<const FPCGMetadataAttribute<int32>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
				};
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<int64>::Id)
			{
				OutType = EPCGPointFilterConstantType::Integer64;
				return [AttributeBase](const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, void* OutData)
				{
					*(static_cast<int64*>(OutData)) = static_cast<const FPCGMetadataAttribute<int64>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
				};
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<float>::Id)
			{
				OutType = EPCGPointFilterConstantType::Float;
				return [AttributeBase](const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, void* OutData)
				{
					*(static_cast<float*>(OutData)) = static_cast<const FPCGMetadataAttribute<float>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
				};
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<double>::Id)
			{
				OutType = EPCGPointFilterConstantType::Float;
				return [AttributeBase](const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, void* OutData)
				{
					*(static_cast<float*>(OutData)) = static_cast<const FPCGMetadataAttribute<double>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
				};
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FVector>::Id)
			{
				OutType = EPCGPointFilterConstantType::Vector;
				return [AttributeBase](const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, void* OutData)
				{
					*(static_cast<FVector*>(OutData)) = static_cast<const FPCGMetadataAttribute<FVector>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
				};
			}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FVector4>::Id)
			{
				OutType = EPCGPointFilterConstantType::Vector4;
				return [AttributeBase](const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, void* OutData)
				{
					*(static_cast<FVector4*>(OutData)) = static_cast<const FPCGMetadataAttribute<FVector4>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
				};
			}
			//else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FQuat>::Id)
			//{
			//	OutType = EPCGPointFilterConstantType::Rotation;
			//	return [AttributeBase](const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, void* OutData)
			//	{
			//		*(static_cast<FQuat*>(OutData)) = static_cast<const FPCGMetadataAttribute<FQuat>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
			//	};
			//}
			//else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FTransform>::Id)
			//{
			//	*(static_cast<FTransform*>(OutData)) = static_cast<const FPCGMetadataAttribute<FTransform>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
			//}
			else if (AttributeBase->GetTypeId() == PCG::Private::MetadataTypes<FString>::Id)
			{
				OutType = EPCGPointFilterConstantType::String;
				return [AttributeBase](const FPCGPoint& InPoint, const UPCGMetadata* InMetadata, void* OutData)
				{
					*(static_cast<FString*>(OutData)) = static_cast<const FPCGMetadataAttribute<FString>*>(AttributeBase)->GetValueFromItemKey(InPoint.MetadataEntry);
				};
			}
			else
			{
				// Log error
				OutType = EPCGPointFilterConstantType::Unknown;
				return [](const FPCGPoint&, const UPCGMetadata*, void*) {};
			}
		}
		else
		{
			// Log error
			OutType = EPCGPointFilterConstantType::Unknown;
			return [](const FPCGPoint&, const UPCGMetadata*, void*) {};
		}
	}

	TFunction<void(const FPCGPoint&, const UPCGMetadata*, void*)> ConstructSourceGetter(
		EPCGPointTargetFilterType TargetFilterType, 
		EPCGPointProperties TargetPointProperty, 
		const UPCGMetadata* TargetMetadata,
		const FName& TargetAttributeName,
		EPCGPointFilterConstantType& OutType)
	{
		if (TargetFilterType == EPCGPointTargetFilterType::Property)
		{
			return ConstructPropertyGetter(TargetPointProperty, OutType);
		}
		else if (TargetFilterType == EPCGPointTargetFilterType::Metadata)
		{
			return ConstructAttributeGetter(TargetMetadata, TargetAttributeName, OutType);
		}
		else
		{
			// log error
			OutType = EPCGPointFilterConstantType::Unknown;
			return [](const FPCGPoint&, const UPCGMetadata*, void*) {};
		}
	}

	TFunction<void(const FPCGPoint&, const UPCGMetadata*, void*)> ConstructThresholdGetter(
		EPCGPointThresholdType ThresholdFilterType, 
		EPCGPointProperties ThresholdPointProperty, 
		const UPCGMetadata* ThresholdMetadata,
		const FName& ThresholdAttributeName, 
		EPCGPointFilterConstantType ThresholdConstantType,
		const int64& Integer64Constant,
		const float& FloatConstant,
		const FVector& VectorConstant,
		const FVector4& Vector4Constant,
		//const FQuat& RotationConstant,
		const FString& StringConstant,
		EPCGPointFilterConstantType& OutType)
	{
		if (ThresholdFilterType == EPCGPointThresholdType::Property)
		{
			return ConstructPropertyGetter(ThresholdPointProperty, OutType);
		}
		else if (ThresholdFilterType == EPCGPointThresholdType::Metadata)
		{
			return ConstructAttributeGetter(ThresholdMetadata, ThresholdAttributeName, OutType);
		}
		else if (ThresholdFilterType == EPCGPointThresholdType::Constant)
		{
			OutType = ThresholdConstantType;

			if (ThresholdConstantType == EPCGPointFilterConstantType::Integer64)
			{
				return [&Integer64Constant](const FPCGPoint&, const UPCGMetadata*, void* OutData)
				{
					*(static_cast<int64*>(OutData)) = Integer64Constant;
				};
			}
			else if (ThresholdConstantType == EPCGPointFilterConstantType::Float)
			{
				return [&FloatConstant](const FPCGPoint&, const UPCGMetadata*, void* OutData)
				{
					*(static_cast<float*>(OutData)) = FloatConstant;
				};
			}
			else if (ThresholdConstantType == EPCGPointFilterConstantType::Vector)
			{
				return [&VectorConstant](const FPCGPoint&, const UPCGMetadata*, void* OutData)
				{
					*(static_cast<FVector*>(OutData)) = VectorConstant;
				};
			}
			else if (ThresholdConstantType == EPCGPointFilterConstantType::Vector4)
			{
				return [&Vector4Constant](const FPCGPoint&, const UPCGMetadata*, void* OutData)
				{
					*(static_cast<FVector4*>(OutData)) = Vector4Constant;
				};
			}
			//else if (ThresholdConstantType == EPCGPointFilterConstantType::Rotation)
			//{
			//	return [&RotationConstant](const FPCGPoint&, const UPCGMetadata*, void* OutData)
			//	{
			//		*(static_cast<FQuat*>(OutData)) = RotationConstant;
			//	};
			//}
			else if (ThresholdConstantType == EPCGPointFilterConstantType::String)
			{
				return [&StringConstant](const FPCGPoint&, const UPCGMetadata*, void* OutData)
				{
					*(static_cast<FString*>(OutData)) = StringConstant;
				};
			}
			else // unknown constant type
			{
				// Log error
				OutType = EPCGPointFilterConstantType::Unknown;
				return [](const FPCGPoint&, const UPCGMetadata*, void*) {};
			}
		}
		else // unknown filter type
		{
			// Log error
			OutType = EPCGPointFilterConstantType::Unknown;
			return [](const FPCGPoint&, const UPCGMetadata*, void*) {};
		}
	}

	template<typename T, typename U>
	struct TemplatedOperator
	{
		TemplatedOperator(EPCGPointFilterOperator Operator)
		{
			if (Operator == EPCGPointFilterOperator::Greater)
			{
				OperatorFn = [](const T& A, const U& B) { return A > B; };
			}
			else if (Operator == EPCGPointFilterOperator::GreaterOrEqual)
			{
				OperatorFn = [](const T& A, const U& B) { return A >= B; };
			}
			else if (Operator == EPCGPointFilterOperator::Lesser)
			{
				OperatorFn = [](const T& A, const U& B) { return A < B; };
			}
			else if (Operator == EPCGPointFilterOperator::LesserOrEqual)
			{
				OperatorFn = [](const T& A, const U& B) { return A <= B; };
			}
			else if (Operator == EPCGPointFilterOperator::Equal)
			{
				OperatorFn = [](const T& A, const U& B) { return A == B; };
			}
			else if (Operator == EPCGPointFilterOperator::NotEqual)
			{
				OperatorFn = [](const T& A, const U& B) { return A != B; };
			}
			else
			{
				OperatorFn = [](const T&, const U&) { return false; };
			}
		}

		bool operator()(const T& A, const U& B) const
		{
			return OperatorFn(A, B);
		}

		TFunction<bool(const T&, const U&)> OperatorFn;
	};

	template<typename U>
	struct TemplatedOperator<FVector, U>
	{
		TemplatedOperator(EPCGPointFilterOperator Operator)
		{
			if (Operator == EPCGPointFilterOperator::Greater)
			{
				OperatorFn = [](const FVector& A, const U& B) { return A.Length() > B; };
			}
			else if (Operator == EPCGPointFilterOperator::GreaterOrEqual)
			{
				OperatorFn = [](const FVector& A, const U& B) { return A.Length() >= B; };
			}
			else if (Operator == EPCGPointFilterOperator::Lesser)
			{
				OperatorFn = [](const FVector& A, const U& B) { return A.Length() < B; };
			}
			else if (Operator == EPCGPointFilterOperator::LesserOrEqual)
			{
				OperatorFn = [](const FVector& A, const U& B) { return A.Length() <= B; };
			}
			else if (Operator == EPCGPointFilterOperator::Equal)
			{
				OperatorFn = [](const FVector& A, const U& B) { return A.Length() == B; };
			}
			else if (Operator == EPCGPointFilterOperator::NotEqual)
			{
				OperatorFn = [](const FVector& A, const U& B) { return A.Length() != B; };
			}
			else
			{
				OperatorFn = [](const FVector&, const U&) { return false; };
			}
		}

		bool operator()(const FVector& A, const U& B) const
		{
			return OperatorFn(A, B);
		}

		TFunction<bool(const FVector&, const U&)> OperatorFn;
	};

	template<typename U>
	struct TemplatedOperator<U, FVector>
	{
		TemplatedOperator(EPCGPointFilterOperator Operator)
		{
			if (Operator == EPCGPointFilterOperator::Greater)
			{
				OperatorFn = [](const U& A, const FVector& B) { return A > B.Length(); };
			}
			else if (Operator == EPCGPointFilterOperator::GreaterOrEqual)
			{
				OperatorFn = [](const U& A, const FVector& B) { return A >= B.Length(); };
			}
			else if (Operator == EPCGPointFilterOperator::Lesser)
			{
				OperatorFn = [](const U& A, const FVector& B) { return A < B.Length(); };
			}
			else if (Operator == EPCGPointFilterOperator::LesserOrEqual)
			{
				OperatorFn = [](const U& A, const FVector& B) { return A <= B.Length(); };
			}
			else if (Operator == EPCGPointFilterOperator::Equal)
			{
				OperatorFn = [](const U& A, const FVector& B) { return A == B.Length(); };
			}
			else if (Operator == EPCGPointFilterOperator::NotEqual)
			{
				OperatorFn = [](const U& A, const FVector& B) { return A != B.Length(); };
			}
			else
			{
				OperatorFn = [](const U&, const FVector&) { return false; };
			}
		}

		bool operator()(const U& A, const FVector& B) const
		{
			return OperatorFn(A, B);
		}

		TFunction<bool(const U&, const FVector&)> OperatorFn;
	};

	template<>
	struct TemplatedOperator<FVector, FVector>
	{
		TemplatedOperator(EPCGPointFilterOperator Operator)
		{
			if (Operator == EPCGPointFilterOperator::Greater)
			{
				OperatorFn = [](const FVector& A, const FVector& B) { return A.X > B.X && A.Y > B.Y && A.Z > B.Z; };
			}
			else if (Operator == EPCGPointFilterOperator::GreaterOrEqual)
			{
				OperatorFn = [](const FVector& A, const FVector& B) { return A.X >= B.X && A.Y >= B.Y && A.Z >= B.Z; };
			}
			else if (Operator == EPCGPointFilterOperator::Lesser)
			{
				OperatorFn = [](const FVector& A, const FVector& B) { return A.X < B.X && A.Y < B.Y && A.Z < B.Z; };
			}
			else if (Operator == EPCGPointFilterOperator::LesserOrEqual)
			{
				OperatorFn = [](const FVector& A, const FVector& B) { return A.X <= B.X && A.Y <= B.Y && A.Z <= B.Z; };
			}
			else if (Operator == EPCGPointFilterOperator::Equal)
			{
				OperatorFn = [](const FVector& A, const FVector& B) { return A == B; };
			}
			else if (Operator == EPCGPointFilterOperator::NotEqual)
			{
				OperatorFn = [](const FVector& A, const FVector& B) { return A != B; };
			}
			else
			{
				OperatorFn = [](const FVector&, const FVector&) { return false; };
			}
		}

		bool operator()(const FVector& A, const FVector& B) const
		{
			return OperatorFn(A, B);
		}

		TFunction<bool(const FVector&, const FVector&)> OperatorFn;
	};

	template<>
	struct TemplatedOperator<FVector4, FVector4>
	{
		TemplatedOperator(EPCGPointFilterOperator Operator)
		{
			if (Operator == EPCGPointFilterOperator::Greater)
			{
				OperatorFn = [](const FVector4& A, const FVector4& B) { return A.X > B.X && A.Y > B.Y && A.Z > B.Z && A.W > B.W; };
			}
			else if (Operator == EPCGPointFilterOperator::GreaterOrEqual)
			{
				OperatorFn = [](const FVector4& A, const FVector4& B) { return A.X >= B.X && A.Y >= B.Y && A.Z >= B.Z && A.W >= B.W; };
			}
			else if (Operator == EPCGPointFilterOperator::Lesser)
			{
				OperatorFn = [](const FVector4& A, const FVector4& B) { return A.X < B.X&& A.Y < B.Y&& A.Z < B.Z && A.W < B.W; };
			}
			else if (Operator == EPCGPointFilterOperator::LesserOrEqual)
			{
				OperatorFn = [](const FVector4& A, const FVector4& B) { return A.X <= B.X && A.Y <= B.Y && A.Z <= B.Z && A.W <= B.W; };
			}
			else if (Operator == EPCGPointFilterOperator::Equal)
			{
				OperatorFn = [](const FVector4& A, const FVector4& B) { return A == B; };
			}
			else if (Operator == EPCGPointFilterOperator::NotEqual)
			{
				OperatorFn = [](const FVector4& A, const FVector4& B) { return A != B; };
			}
			else
			{
				OperatorFn = [](const FVector4&, const FVector4&) { return false; };
			}
		}

		bool operator()(const FVector& A, const FVector& B) const
		{
			return OperatorFn(A, B);
		}

		TFunction<bool(const FVector&, const FVector&)> OperatorFn;
	};

	TFunction<bool(const FPCGPoint&, const UPCGMetadata*, const FPCGPoint&, const UPCGMetadata*)> ConstructConcreteOperator(
		const TFunction<void(const FPCGPoint&, const UPCGMetadata*, void*)>& SourceGetter,
		EPCGPointFilterConstantType SourceType,
		const TFunction<void(const FPCGPoint&, const UPCGMetadata*, void*)>& ThresholdGetter,
		EPCGPointFilterConstantType ThresholdType,
		EPCGPointFilterOperator Operator,
		bool& bOutOperatorCreatedCorrectly)
	{
		bOutOperatorCreatedCorrectly = true;

#define CONCRETE_OPERATOR_SHORTHAND(TypeEnumA, TypeA, TypeEnumB, TypeB) \
		if(SourceType == EPCGPointFilterConstantType::TypeEnumA && ThresholdType == EPCGPointFilterConstantType::TypeEnumB) \
		{ \
			TemplatedOperator<TypeA, TypeB> OperatorFn(Operator); \
			\
			return[&SourceGetter, &ThresholdGetter, OperatorFn](const FPCGPoint& InTargetPoint, const UPCGMetadata* InTargetMetadata, const FPCGPoint& InThresholdPoint, const UPCGMetadata* InThresholdMetadata) \
			{ \
				TypeA A; \
				TypeB B; \
				\
				SourceGetter(InTargetPoint, InTargetMetadata, &A); \
				ThresholdGetter(InThresholdPoint, InThresholdMetadata, &B); \
				return OperatorFn(A, B); \
			}; \
		}

		CONCRETE_OPERATOR_SHORTHAND(Integer64, int64, Integer64, int64)
		CONCRETE_OPERATOR_SHORTHAND(Integer64, int64, Vector, FVector)
		CONCRETE_OPERATOR_SHORTHAND(Float, float, Float, float)
		CONCRETE_OPERATOR_SHORTHAND(Float, float, Vector, FVector)
		CONCRETE_OPERATOR_SHORTHAND(Vector, FVector, Vector, FVector)
		CONCRETE_OPERATOR_SHORTHAND(Vector, FVector, Integer64, int64)
		CONCRETE_OPERATOR_SHORTHAND(Vector, FVector, Float, float)
		CONCRETE_OPERATOR_SHORTHAND(Vector4, FVector4, Vector4, FVector4)
		//CONCRETE_OPERATOR_SHORTHAND(Rotation, FQuat, Rotation, FQuat)
		CONCRETE_OPERATOR_SHORTHAND(String, FString, String, FString)

#undef CONCRETE_OPERATOR_SHORTHAND

		bOutOperatorCreatedCorrectly = false;
		return [](const FPCGPoint&, const UPCGMetadata*, const FPCGPoint&, const UPCGMetadata*) { return false; };
	}
}

TArray<FPCGPinProperties> UPCGPointFilterSettings::InputPinProperties() const
{
	TArray<FPCGPinProperties> PinProperties;
	PinProperties.Emplace(PCGPointFilterConstants::DataToFilterLabel, EPCGDataType::Any);
	PinProperties.Emplace(PCGPointFilterConstants::FilterLabel, EPCGDataType::Spatial);

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

bool FPCGPointFilterElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGPointFilterElement::Execute);
	check(Context);

	const bool bHasInFilterOutputPin = Context->Node && Context->Node->IsOutputPinConnected(PCGPointFilterConstants::InFilterLabel);
	const bool bHasOutsideFilterOutputPin = Context->Node && Context->Node->IsOutputPinConnected(PCGPointFilterConstants::OutFilterLabel);

	// Early out
	if (!bHasInFilterOutputPin && !bHasOutsideFilterOutputPin)
	{
		return true;
	}

	const UPCGPointFilterSettings* Settings = Context->GetInputSettings<UPCGPointFilterSettings>();
	check(Settings);

	TArray<FPCGTaggedData> DataToFilter = Context->InputData.GetInputsByPin(PCGPointFilterConstants::DataToFilterLabel);
	UPCGParamData* Params = Context->InputData.GetParams(); //TODO: by pin?
	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;
	TArray<FPCGTaggedData> FilterData = Context->InputData.GetInputsByPin(PCGPointFilterConstants::FilterLabel);
	FilterData.Append(Context->InputData.GetParamsByPin(PCGPointFilterConstants::FilterLabel));

	// Forward any non-input data
	Outputs.Append(Context->InputData.GetAllSettings());

	const EPCGPointFilterOperator Operator = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, Operator), Settings->Operator, Params);
	const EPCGPointTargetFilterType TargetFilterType = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, TargetFilterType), Settings->TargetFilterType, Params);
	const EPCGPointProperties TargetPointProperty = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, TargetPointProperty), Settings->TargetPointProperty, Params);
	const FName TargetAttributeName = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, TargetAttributeName), Settings->TargetAttributeName, Params);

	const EPCGPointThresholdType ThresholdFilterType = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, ThresholdFilterType), Settings->ThresholdFilterType, Params);
	const EPCGPointProperties ThresholdPointProperty = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, ThresholdPointProperty), Settings->ThresholdPointProperty, Params);
	const FName ThresholdAttributeName = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, ThresholdAttributeName), Settings->ThresholdAttributeName, Params);
	const EPCGPointFilterConstantType ThresholdConstantType = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, ThresholdConstantType), Settings->ThresholdConstantType, Params);

	const int64 Integer64Constant = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, Integer64Constant), Settings->Integer64Constant, Params);
	const float FloatConstant = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, FloatConstant), Settings->FloatConstant, Params);
	const FVector VectorConstant = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, VectorConstant), Settings->VectorConstant, Params);
	const FVector4 Vector4Constant = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, Vector4Constant), Settings->Vector4Constant, Params);
	//const FQuat RotationConstant = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, RotationConstant), Settings->RotationConstant, Params);
	const FString StringConstant = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, StringConstant), Settings->StringConstant, Params);

	bool bUseSpatialQuery = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGPointFilterSettings, bUseSpatialQuery), Settings->bUseSpatialQuery, Params);

	// Validate basic input data
	if (ThresholdFilterType == EPCGPointThresholdType::Property && !FilterData.IsEmpty() && Cast<UPCGSpatialData>(FilterData[0].Data) == nullptr)
	{
		PCGE_LOG(Error, "Cannot filter by property on a non-spatial filter data");
		return true;
	}

	if (ThresholdFilterType == EPCGPointThresholdType::Constant && !FilterData.IsEmpty())
	{
		PCGE_LOG(Warning, "Point filter uses a constant but has filter data that will be ignored");
	}

	for (const FPCGTaggedData& Input : DataToFilter)
	{
		const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Input.Data);
		check(SpatialInput);

		const UPCGPointData* OriginalData = SpatialInput->ToPointData(Context);
		if (!OriginalData)
		{
			PCGE_LOG(Error, "Unable to get point data from input");
			continue;
		}

		const TArray<FPCGPoint>& TargetPoints = OriginalData->GetPoints();
		const UPCGMetadata* TargetMetadata = OriginalData->ConstMetadata();

		const UPCGSpatialData* ThresholdSpatialData = nullptr;
		const TArray<FPCGPoint>* ThresholdPoints = nullptr;
		const UPCGMetadata* ThresholdMetadata = nullptr;
		UPCGMetadata* TemporaryMetadata = nullptr;

		if (FilterData.Num() > 0)
		{
			if (const UPCGSpatialData* SpatialDataFilter = Cast<const UPCGSpatialData>(FilterData[0].Data))
			{
				ThresholdSpatialData = SpatialDataFilter;
				ThresholdMetadata = ThresholdSpatialData->ConstMetadata();

				if (bUseSpatialQuery && ThresholdMetadata && ThresholdFilterType == EPCGPointThresholdType::Metadata)
				{
					TemporaryMetadata = NewObject<UPCGMetadata>();
					TemporaryMetadata->Initialize(ThresholdMetadata);
				}
				else if(!bUseSpatialQuery)
				{
					const UPCGPointData* ThresholdData = SpatialDataFilter->ToPointData();
					ThresholdPoints = &ThresholdData->GetPoints();
				}
			}
			else if (const UPCGParamData* ParamDataFilter = Cast<const UPCGParamData>(FilterData[0].Data))
			{
				ThresholdMetadata = ParamDataFilter->ConstMetadata();
			}
		}
		else
		{
			// Use self as filter
			ThresholdMetadata = TargetMetadata;
			ThresholdPoints = &TargetPoints;
		}

		const FName LocalTargetAttributeName = ((TargetAttributeName != NAME_None || !TargetMetadata) ? TargetAttributeName : TargetMetadata->GetLatestAttributeNameOrNone());

		// Additional validation
		if (TargetFilterType == EPCGPointTargetFilterType::Metadata)
		{
			if (!TargetMetadata)
			{
				PCGE_LOG(Error, "Target data to filter has no metadata which is required to filter by metadata");
				continue;
			}
			else if (!TargetMetadata->GetConstAttribute(LocalTargetAttributeName))
			{
				PCGE_LOG(Error, "Target metadata does not have the %s attribute", *LocalTargetAttributeName.ToString());
				continue;
			}
		}

		const FName LocalThresholdAttributeName = ((ThresholdAttributeName != NAME_None || !ThresholdMetadata) ? ThresholdAttributeName : ThresholdMetadata->GetLatestAttributeNameOrNone());
		
		if (ThresholdFilterType == EPCGPointThresholdType::Metadata)
		{
			if (!ThresholdMetadata)
			{
				PCGE_LOG(Error, "Filter data has no metadata which is required to filter by metadata");
				continue;
			}
			else if (!ThresholdMetadata->GetConstAttribute(LocalThresholdAttributeName))
			{
				PCGE_LOG(Error, "Filter metadata does not have the %s attribute", *LocalThresholdAttributeName.ToString());
				continue;
			}
		}
		else if (ThresholdFilterType == EPCGPointThresholdType::Property)
		{
			if (!bUseSpatialQuery && (!ThresholdPoints || (ThresholdPoints->Num() > 1 && TargetPoints.Num() > 0 && ThresholdPoints->Num() != TargetPoints.Num())))
			{
				PCGE_LOG(Error, "Filter data points count (%d) mismatch vs the data to filter (%d)", (ThresholdPoints ? ThresholdPoints->Num() : 0), TargetPoints.Num());
				continue;
			}

			if (!bUseSpatialQuery && !ThresholdSpatialData)
			{
				PCGE_LOG(Error, "Filter data cannot be used as spatial data");
				continue;
			}
		}

		// Build our functions
		EPCGPointFilterConstantType TargetType = EPCGPointFilterConstantType::Unknown;
		EPCGPointFilterConstantType ThresholdType = EPCGPointFilterConstantType::Unknown;

		TFunction<void(const FPCGPoint&, const UPCGMetadata*, void*)> SourceGetter = PCGPointFilterHelpers::ConstructSourceGetter(
			TargetFilterType, 
			TargetPointProperty, 
			TargetMetadata,
			LocalTargetAttributeName,
			TargetType);

		if (TargetType == EPCGPointFilterConstantType::Unknown)
		{
			PCGE_LOG(Error, "Unable to generate the data value getter");
			continue;
		}

		// B - a getter that returns the value for the threshold data
		TFunction<void(const FPCGPoint&, const UPCGMetadata*, void*)> ThresholdGetter = PCGPointFilterHelpers::ConstructThresholdGetter(
			ThresholdFilterType,
			ThresholdPointProperty,
			ThresholdMetadata,
			LocalThresholdAttributeName,
			ThresholdConstantType,
			Integer64Constant,
			FloatConstant,
			VectorConstant,
			Vector4Constant,
			//RotationConstant,
			StringConstant,
			ThresholdType);

		if (ThresholdType == EPCGPointFilterConstantType::Unknown)
		{
			PCGE_LOG(Error, "Unable to generate the filter value getter");
			continue;
		}

		// C - the operator
		bool bConcreteOperatorConstructedCorrectly = false;
		TFunction<bool(const FPCGPoint&, const UPCGMetadata*, const FPCGPoint&, const UPCGMetadata*)> ConcreteOperator = PCGPointFilterHelpers::ConstructConcreteOperator(
			SourceGetter,
			TargetType,
			ThresholdGetter,
			ThresholdType,
			Operator,
			bConcreteOperatorConstructedCorrectly);

		if (!bConcreteOperatorConstructedCorrectly)
		{
			PCGE_LOG(Error, "Unable to generate the operator (most likely data types mismatch");
			continue;
		}

		UPCGPointData* InFilterData = NewObject<UPCGPointData>();
		InFilterData->InitializeFromData(OriginalData);
		TArray<FPCGPoint>& InPoints = InFilterData->GetMutablePoints();

		UPCGPointData* OutFilterData = NewObject<UPCGPointData>();
		OutFilterData->InitializeFromData(OriginalData);
		TArray<FPCGPoint>& OutPoints = OutFilterData->GetMutablePoints();

		FPCGAsync::AsyncPointFilterProcessing(Context, TargetPoints.Num(), InPoints, OutPoints, [&TargetPoints, TargetMetadata, ThresholdSpatialData, ThresholdPoints, ThresholdMetadata, TemporaryMetadata, &ConcreteOperator](int32 Index, FPCGPoint& InFilterPoint, FPCGPoint& OutFilterPoint)
		{
			const FPCGPoint& SourcePoint = TargetPoints[Index];

			bool bSkipTest = false;
			FPCGPoint ThresholdPoint;
			if (ThresholdPoints) // not null only in the direct match case
			{
				ThresholdPoint = (*ThresholdPoints)[(ThresholdPoints->Num() > 1) ? Index : 0];
			}
			else if(ThresholdSpatialData)
			{
				bSkipTest = !ThresholdSpatialData->SamplePoint(SourcePoint.Transform, SourcePoint.GetLocalBounds(), ThresholdPoint, TemporaryMetadata);
			}
			else // Only metadata entry (param)
			{
				ThresholdPoint.MetadataEntry = 0;
			}

			// Apply operator
			bool bPointInFilter = bSkipTest || ConcreteOperator(SourcePoint, TargetMetadata, ThresholdPoint, (TemporaryMetadata ? TemporaryMetadata : ThresholdMetadata));
			if (bPointInFilter)
			{
				InFilterPoint = SourcePoint;
				return true;
			}
			else
			{
				OutFilterPoint = SourcePoint;
				return false;
			}
		});

		// Push the filtered data to the output data.
		FPCGTaggedData& InFilterTaggedData = Outputs.Emplace_GetRef();
		InFilterTaggedData.Data = InFilterData;
		InFilterTaggedData.Tags = Input.Tags;
		InFilterTaggedData.Pin = PCGPointFilterConstants::InFilterLabel;

		FPCGTaggedData& OutFilterTaggedData = Outputs.Emplace_GetRef();
		OutFilterTaggedData.Data = OutFilterData;
		OutFilterTaggedData.Tags = Input.Tags;
		OutFilterTaggedData.Pin = PCGPointFilterConstants::OutFilterLabel;
	}

	return true;
}