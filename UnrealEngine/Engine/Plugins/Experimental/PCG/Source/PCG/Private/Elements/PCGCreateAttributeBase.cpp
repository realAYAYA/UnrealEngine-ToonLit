// Copyright Epic Games, Inc. All Rights Reserved.

#include "Elements/PCGCreateAttributeBase.h"

#include "PCGParamData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/PCGMetadata.h"
#include "Helpers/PCGSettingsHelpers.h"

namespace PCGAttributeCreationBaseSettings
{
	template <typename Func>
	decltype(auto) Dispatcher(const UPCGAttributeCreationBaseSettings* Settings, UPCGParamData* Params, Func Callback)
	{
		using ReturnType = decltype(Callback(double{}));

		switch (Settings->Type)
		{
		case EPCGMetadataTypes::Integer64:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, IntValue, Params));
		case EPCGMetadataTypes::Double:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, DoubleValue, Params));
		case EPCGMetadataTypes::Vector2:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, Vector2Value, Params));
		case EPCGMetadataTypes::Vector:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, VectorValue, Params));
		case EPCGMetadataTypes::Vector4:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, Vector4Value, Params));
		case EPCGMetadataTypes::Quaternion:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, QuatValue, Params));
		case EPCGMetadataTypes::Transform:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, TransformValue, Params));
		case EPCGMetadataTypes::String:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, StringValue, Params));
		case EPCGMetadataTypes::Boolean:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, BoolValue, Params));
		case EPCGMetadataTypes::Rotator:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, RotatorValue, Params));
		case EPCGMetadataTypes::Name:
			return Callback(PCG_GET_OVERRIDEN_VALUE(Settings, NameValue, Params));
		default:
			// ReturnType{} is invalid if ReturnType is void
			if constexpr (std::is_same_v<ReturnType, void>)
			{
				return;
			}
			else
			{
				return ReturnType{};
			}
		}
	}
}

FPCGMetadataAttributeBase* UPCGAttributeCreationBaseSettings::ClearOrCreateAttribute(UPCGMetadata* Metadata, UPCGParamData* Params) const
{
	check(Metadata);

	auto CreateAttribute = [this, Metadata](auto&& Value) -> FPCGMetadataAttributeBase*
	{
		return PCGMetadataElementCommon::ClearOrCreateAttribute(Metadata, OutputAttributeName, Value);
	};

	return PCGAttributeCreationBaseSettings::Dispatcher(this, Params, CreateAttribute);
}

PCGMetadataEntryKey UPCGAttributeCreationBaseSettings::SetAttribute(FPCGMetadataAttributeBase* Attribute, UPCGMetadata* Metadata, PCGMetadataEntryKey EntryKey, UPCGParamData* Params) const
{
	check(Attribute && Metadata);

	auto SetAttribute = [this, Attribute, EntryKey, Metadata](auto&& Value) -> PCGMetadataEntryKey
	{
		using AttributeType = std::remove_reference_t<decltype(Value)>;

		check(Attribute->GetTypeId() == PCG::Private::MetadataTypes<AttributeType>::Id);

		const PCGMetadataEntryKey FinalKey = (EntryKey == PCGInvalidEntryKey) ? Metadata->AddEntry() : EntryKey;

		static_cast<FPCGMetadataAttribute<AttributeType>*>(Attribute)->SetValue(FinalKey, Value);

		return FinalKey;
	};

	return PCGAttributeCreationBaseSettings::Dispatcher(this, Params, SetAttribute);
}

void UPCGAttributeCreationBaseSettings::SetAttribute(FPCGMetadataAttributeBase* Attribute, UPCGMetadata* Metadata, TArray<FPCGPoint>& Points, UPCGParamData* Params) const
{
	if (Points.IsEmpty())
	{
		return;
	}

	check(Attribute && Metadata);

	auto SetAttribute = [this, Attribute, Metadata, &Points](auto&& Value) -> void
	{
		using AttributeType = std::remove_reference_t<decltype(Value)>;

		check(Attribute->GetTypeId() == PCG::Private::MetadataTypes<AttributeType>::Id);

		FPCGMetadataAttribute<AttributeType>* TypedAttribute = static_cast<FPCGMetadataAttribute<AttributeType>*>(Attribute);

		for (FPCGPoint& Point : Points)
		{
			PCGMetadataEntryKey EntryKey = (Point.MetadataEntry == PCGInvalidEntryKey) ? Metadata->AddEntry() : Point.MetadataEntry;

			TypedAttribute->SetValue(EntryKey, Value);

			Point.MetadataEntry = EntryKey;
		}
	};

	PCGAttributeCreationBaseSettings::Dispatcher(this, Params, SetAttribute);
}

FName UPCGAttributeCreationBaseSettings::AdditionalTaskName() const
{
	const FString Name = OutputAttributeName.ToString();

	switch (Type)
	{
	case EPCGMetadataTypes::Integer64:
		return FName(FString::Printf(TEXT("%s: %ll"), *Name, IntValue));
	case EPCGMetadataTypes::Double:
		return FName(FString::Printf(TEXT("%s: %.2f"), *Name, DoubleValue));
	case EPCGMetadataTypes::String:
		return FName(FString::Printf(TEXT("%s: \"%s\""), *Name, *StringValue));
	case EPCGMetadataTypes::Name:
		return FName(FString::Printf(TEXT("%s: N(\"%s\")"), *Name, *NameValue.ToString()));
	case EPCGMetadataTypes::Vector2:
		return FName(FString::Printf(TEXT("%s: V(%.2f, %.2f)"), *Name, Vector2Value.X, Vector2Value.Y));
	case EPCGMetadataTypes::Vector:
		return FName(FString::Printf(TEXT("%s: V(%.2f, %.2f, %.2f)"), *Name, VectorValue.X, VectorValue.Y, VectorValue.Z));
	case EPCGMetadataTypes::Vector4:
		return FName(FString::Printf(TEXT("%s: V(%.2f, %.2f, %.2f, %.2f)"), *Name, Vector4Value.X, Vector4Value.Y, Vector4Value.Z, Vector4Value.W));
	case EPCGMetadataTypes::Rotator:
		return FName(FString::Printf(TEXT("%s: R(%.2f, %.2f, %.2f)"), *Name, RotatorValue.Roll, RotatorValue.Pitch, RotatorValue.Yaw));
	case EPCGMetadataTypes::Quaternion:
		return FName(FString::Printf(TEXT("%s: Q(%.2f, %.2f, %.2f, %.2f)"), *Name, QuatValue.X, QuatValue.Y, QuatValue.Z, QuatValue.W));
	case EPCGMetadataTypes::Transform:
		return FName(FString::Printf(TEXT("%s: Transform"), *Name));
	case EPCGMetadataTypes::Boolean:
		return FName(FString::Printf(TEXT("%s: %s"), *Name, (BoolValue ? TEXT("True") : TEXT("False"))));
	default:
		return NAME_None;
	}
}