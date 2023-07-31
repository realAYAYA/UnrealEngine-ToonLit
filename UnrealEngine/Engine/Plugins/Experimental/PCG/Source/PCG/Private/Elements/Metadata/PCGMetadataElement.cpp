// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/Metadata/PCGMetadataElement.h"
#include "Data/PCGPointData.h"
#include "Helpers/PCGSettingsHelpers.h"
#include "Metadata/PCGMetadata.h"
#include "Metadata/PCGMetadataAttribute.h"
#include "Metadata/PCGMetadataAttributeTpl.h"

namespace PCGMetadataOperations
{
	// Getter and setter for properties.
	// Need to specify the property type and the getter/setter.
	// Will work if AttributeType and PropertyType are the same, or if we can construct one from the other.
	template <typename T>
	bool SetValue(TArray<FPCGPoint>& InPoints, FPCGMetadataAttributeBase* AttributeBase, UPCGMetadata* Metadata, TFunctionRef<T(const FPCGPoint& InPoint)> PropGetter)
	{
		if (!AttributeBase)
		{
			return false;
		}

		auto Func = [&InPoints, AttributeBase, Metadata, &PropGetter](auto DummyValue) -> bool
		{
			using AttributeType = decltype(DummyValue);

			FPCGMetadataAttribute<AttributeType>* Attribute = static_cast<FPCGMetadataAttribute<AttributeType>*>(AttributeBase);

			for (FPCGPoint& Point : InPoints)
			{
				if constexpr (std::is_same_v<AttributeType, T>)
				{
					Metadata->InitializeOnSet(Point.MetadataEntry);
					Attribute->SetValue(Point.MetadataEntry, PropGetter(Point));
				}
				else if constexpr (std::is_constructible_v<AttributeType, T>)
				{
					Metadata->InitializeOnSet(Point.MetadataEntry);
					Attribute->SetValue(Point.MetadataEntry, AttributeType(PropGetter(Point)));
				}
				else
				{
					return false;
				}
			}

			return true;
		};

		return PCGMetadataAttribute::CallbackWithRightType(AttributeBase->GetTypeId(), Func);
	}

	template <typename T>
	bool SetValue(const FPCGMetadataAttributeBase* AttributeBase, TArray<FPCGPoint>& InPoints, TFunctionRef<void(FPCGPoint& OutPoint, const T& InValue)> PropSetter)
	{
		if (!AttributeBase)
		{
			return false;
		}

		auto Func = [&InPoints, AttributeBase, &PropSetter](auto DummyValue) -> bool
		{
			using AttributeType = decltype(DummyValue);

			const FPCGMetadataAttribute<AttributeType>* Attribute = static_cast<const FPCGMetadataAttribute<AttributeType>*>(AttributeBase);

			for (FPCGPoint& Point : InPoints)
			{
				if constexpr (std::is_same_v<T, AttributeType>)
				{
					PropSetter(Point, Attribute->GetValueFromItemKey(Point.MetadataEntry));
				}
				else if constexpr (std::is_constructible_v<T, AttributeType>)
				{
					PropSetter(Point, T(Attribute->GetValueFromItemKey(Point.MetadataEntry)));
				}
				else
				{
					return false;
				}
			}

			return true;
		};

		return PCGMetadataAttribute::CallbackWithRightType(AttributeBase->GetTypeId(), Func);
	}
}

FPCGElementPtr UPCGMetadataOperationSettings::CreateElement() const
{
	return MakeShared<FPCGMetadataOperationElement>();
}

bool FPCGMetadataOperationElement::ExecuteInternal(FPCGContext* Context) const
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FPCGMetadataOperationElement::Execute);

	const UPCGMetadataOperationSettings* Settings = Context->GetInputSettings<UPCGMetadataOperationSettings>();
	check(Settings);

	TArray<FPCGTaggedData> Inputs = Context->InputData.GetInputs();
	UPCGParamData* Params = Context->InputData.GetParams();

	TArray<FPCGTaggedData>& Outputs = Context->OutputData.TaggedData;

	const FName SourceAttribute = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGMetadataOperationSettings, SourceAttribute), Settings->SourceAttribute, Params);
	const EPCGPointProperties PointProperty = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGMetadataOperationSettings, PointProperty), Settings->PointProperty, Params);
	const FName DestinationAttribute = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGMetadataOperationSettings, DestinationAttribute), Settings->DestinationAttribute, Params);
	const EPCGMetadataOperationTarget Target = PCGSettingsHelpers::GetValue(GET_MEMBER_NAME_CHECKED(UPCGMetadataOperationSettings, Target), Settings->Target, Params);

	// Forward any non-input data
	Outputs.Append(Context->InputData.GetAllSettings());

	for (const FPCGTaggedData& Input : Inputs)
	{
		FPCGTaggedData& Output = Outputs.Add_GetRef(Input);

		const UPCGSpatialData* SpatialInput = Cast<const UPCGSpatialData>(Input.Data);

		if (!SpatialInput)
		{
			PCGE_LOG(Error, "Invalid input data");
			continue;
		}

		const UPCGPointData* OriginalData = SpatialInput->ToPointData(Context);

		if (!OriginalData)
		{
			PCGE_LOG(Error, "Unable to get point data from input");
			continue;
		}

		if (!OriginalData->Metadata)
		{
			PCGE_LOG(Warning, "Input has no metadata");
			continue;
		}

		const FName LocalSourceAttribute = ((SourceAttribute != NAME_None) ? SourceAttribute : OriginalData->Metadata->GetLatestAttributeNameOrNone());

		// Check if the attribute exists
		if ((Target == EPCGMetadataOperationTarget::AttributeToProperty || Target == EPCGMetadataOperationTarget::AttributeToAttribute) && !OriginalData->Metadata->HasAttribute(LocalSourceAttribute))
		{
			PCGE_LOG(Warning, "Input does not have the %s attribute", *LocalSourceAttribute.ToString());
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

		if (Target == EPCGMetadataOperationTarget::PropertyToAttribute)
		{
			if (PointProperty == EPCGPointProperties::Density)
			{
				auto DensityGetter = [](const FPCGPoint& InPoint) { return InPoint.Density; };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateFloatAttribute(DestinationAttribute, 0.0f, /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if(!PCGMetadataOperations::SetValue<float>(SampledPoints, AttributeBase, SampledData->Metadata, DensityGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::BoundsMin)
			{
				auto ExtentsGetter = [](const FPCGPoint& InPoint) { return InPoint.BoundsMin; };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateVectorAttribute(DestinationAttribute, FVector::Zero(), /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if (!PCGMetadataOperations::SetValue<FVector>(SampledPoints, AttributeBase, SampledData->Metadata, ExtentsGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::BoundsMax)
			{
				auto ExtentsGetter = [](const FPCGPoint& InPoint) { return InPoint.BoundsMax; };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateVectorAttribute(DestinationAttribute, FVector::Zero(), /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if (!PCGMetadataOperations::SetValue<FVector>(SampledPoints, AttributeBase, SampledData->Metadata, ExtentsGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Extents)
			{
				auto ExtentsGetter = [](const FPCGPoint& InPoint) { return InPoint.GetExtents(); };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateVectorAttribute(DestinationAttribute, FVector::Zero() , /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if(!PCGMetadataOperations::SetValue<FVector>(SampledPoints, AttributeBase, SampledData->Metadata, ExtentsGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
			else if(PointProperty == EPCGPointProperties::Color)
			{
				auto ColorGetter = [](const FPCGPoint& InPoint) { return InPoint.Color; };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateVector4Attribute(DestinationAttribute, FVector4::Zero(), /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if(!PCGMetadataOperations::SetValue<FVector4>(SampledPoints, AttributeBase, SampledData->Metadata, ColorGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Position)
			{
				auto PositionGetter = [](const FPCGPoint& InPoint) { return InPoint.Transform.GetLocation(); };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateVectorAttribute(DestinationAttribute, FVector::Zero(), /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if (!PCGMetadataOperations::SetValue<FVector>(SampledPoints, AttributeBase, SampledData->Metadata, PositionGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Rotation)
			{
				auto RotationGetter = [](const FPCGPoint& InPoint) { return InPoint.Transform.GetRotation(); };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateQuatAttribute(DestinationAttribute, FQuat::Identity, /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if (!PCGMetadataOperations::SetValue<FQuat>(SampledPoints, AttributeBase, SampledData->Metadata, RotationGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Scale)
			{
				auto ScaleGetter = [](const FPCGPoint& InPoint) { return InPoint.Transform.GetScale3D(); };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateVectorAttribute(DestinationAttribute, FVector::One(), /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if (!PCGMetadataOperations::SetValue<FVector>(SampledPoints, AttributeBase, SampledData->Metadata, ScaleGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Transform)
			{
				auto TransformGetter = [](const FPCGPoint& InPoint) { return InPoint.Transform; };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateTransformAttribute(DestinationAttribute, FTransform::Identity, /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if (!PCGMetadataOperations::SetValue<FTransform>(SampledPoints, AttributeBase, SampledData->Metadata, TransformGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Steepness)
			{
				auto SteepnessGetter = [](const FPCGPoint& InPoint) { return InPoint.Steepness; };

				if (!SampledData->Metadata->HasAttribute(DestinationAttribute))
				{
					SampledData->Metadata->CreateFloatAttribute(DestinationAttribute, 0.5f, /*bAllowsInterpolation=*/true, /*bOverrideParent=*/true);
				}

				FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetMutableAttribute(DestinationAttribute);
				if (!PCGMetadataOperations::SetValue<float>(SampledPoints, AttributeBase, SampledData->Metadata, SteepnessGetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *DestinationAttribute.ToString());
				}
			}
		}
		else if(Target == EPCGMetadataOperationTarget::AttributeToProperty) // Attribute to property
		{
			if (PointProperty == EPCGPointProperties::Density)
			{
				auto DensitySetter = [](FPCGPoint& InPoint, const float& InValue) { InPoint.Density = InValue; };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if(!PCGMetadataOperations::SetValue<float>(AttributeBase, SampledPoints, DensitySetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::BoundsMin)
			{
				auto ExtentsSetter = [](FPCGPoint& InPoint, const FVector& InValue) { InPoint.BoundsMin = InValue; };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if (!PCGMetadataOperations::SetValue<FVector>(AttributeBase, SampledPoints, ExtentsSetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::BoundsMax)
			{
				auto ExtentsSetter = [](FPCGPoint& InPoint, const FVector& InValue) { InPoint.BoundsMax = InValue; };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if (!PCGMetadataOperations::SetValue<FVector>(AttributeBase, SampledPoints, ExtentsSetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Extents)
			{
				auto ExtentsSetter = [](FPCGPoint& InPoint, const FVector& InValue) { InPoint.SetExtents(InValue); };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if(!PCGMetadataOperations::SetValue<FVector>(AttributeBase, SampledPoints, ExtentsSetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Color)
			{
				auto ColorSetter = [](FPCGPoint& InPoint, const FVector4& InValue) { InPoint.Color = InValue; };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if(!PCGMetadataOperations::SetValue<FVector4>(AttributeBase, SampledPoints, ColorSetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Position)
			{
				auto PositionSetter = [](FPCGPoint& InPoint, const FVector& InValue) { InPoint.Transform.SetLocation(InValue); };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if (!PCGMetadataOperations::SetValue<FVector>(AttributeBase, SampledPoints, PositionSetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Rotation)
			{
				auto RotationSetter = [](FPCGPoint& InPoint, const FQuat& InValue) { InPoint.Transform.SetRotation(InValue.GetNormalized()); };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if (!PCGMetadataOperations::SetValue<FQuat>(AttributeBase, SampledPoints, RotationSetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Scale)
			{
				auto ScaleSetter = [](FPCGPoint& InPoint, const FVector& InValue) { InPoint.Transform.SetScale3D(InValue); };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if (!PCGMetadataOperations::SetValue<FVector>(AttributeBase, SampledPoints, ScaleSetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Transform)
			{
				auto TransformSetter = [](FPCGPoint& InPoint, const FTransform& InValue) { InPoint.Transform = InValue; };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if (!PCGMetadataOperations::SetValue<FTransform>(AttributeBase, SampledPoints, TransformSetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
			else if (PointProperty == EPCGPointProperties::Steepness)
			{
				auto SteepnessSetter = [](FPCGPoint& InPoint, const float& InValue) { InPoint.Steepness = InValue; };

				const FPCGMetadataAttributeBase* AttributeBase = SampledData->Metadata->GetConstAttribute(LocalSourceAttribute);
				if (!PCGMetadataOperations::SetValue<float>(AttributeBase, SampledPoints, SteepnessSetter))
				{
					PCGE_LOG(Error, "Attribute %s already exists but its type is not compatible", *LocalSourceAttribute.ToString());
				}
			}
		}
		else // Attribute to attribute
		{
			SampledData->Metadata->CopyExistingAttribute(LocalSourceAttribute, DestinationAttribute);
		}
	}

	return true;
}