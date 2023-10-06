// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphValueContainer.h"

#include "MovieGraphCommon.h"

// The property bag has one "default" property in it
const FName UMovieGraphValueContainer::PropertyBagDefaultPropertyName("Default");

UMovieGraphValueContainer::UMovieGraphValueContainer()
{
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Add a default float property if one does not exist already
		if (Value.GetNumPropertiesInBag() == 0)
		{
			Value.AddProperty(PropertyBagDefaultPropertyName, EPropertyBagPropertyType::Float);
		}
	}
}

bool UMovieGraphValueContainer::GetValueBool(bool& bOutValue) const
{
	TValueOrError<bool, EPropertyBagResult> Result = Value.GetValueBool(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<bool>(Result, bOutValue);
}

bool UMovieGraphValueContainer::GetValueByte(uint8& OutValue) const
{
	TValueOrError<uint8, EPropertyBagResult> Result = Value.GetValueByte(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<uint8>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueInt32(int32& OutValue) const
{
	TValueOrError<int32, EPropertyBagResult> Result = Value.GetValueInt32(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<int32>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueInt64(int64& OutValue) const
{
	TValueOrError<int64, EPropertyBagResult> Result = Value.GetValueInt64(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<int64>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueFloat(float& OutValue) const
{
	TValueOrError<float, EPropertyBagResult> Result = Value.GetValueFloat(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<float>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueDouble(double& OutValue) const
{
	TValueOrError<double, EPropertyBagResult> Result = Value.GetValueDouble(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<double>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueName(FName& OutValue) const
{
	TValueOrError<FName, EPropertyBagResult> Result = Value.GetValueName(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<FName>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueString(FString& OutValue) const
{
	TValueOrError<FString, EPropertyBagResult> Result = Value.GetValueString(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<FString>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueText(FText& OutValue) const
{
	TValueOrError<FText, EPropertyBagResult> Result = Value.GetValueText(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<FText>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueEnum(uint8& OutValue, const UEnum* RequestedEnum) const
{
	TValueOrError<uint8, EPropertyBagResult> Result = Value.GetValueEnum(PropertyBagDefaultPropertyName, RequestedEnum);
	return UE::MovieGraph::Private::GetOptionalValue<uint8>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueStruct(FStructView& OutValue, const UScriptStruct* RequestedStruct) const
{
	TValueOrError<FStructView, EPropertyBagResult> Result = Value.GetValueStruct(PropertyBagDefaultPropertyName, RequestedStruct);
	return UE::MovieGraph::Private::GetOptionalValue<FStructView>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueObject(UObject* OutValue, const UClass* RequestedClass) const
{
	TValueOrError<UObject*, EPropertyBagResult> Result = Value.GetValueObject(PropertyBagDefaultPropertyName, RequestedClass);
	return UE::MovieGraph::Private::GetOptionalValue<UObject*>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueClass(UClass* OutValue) const
{
	TValueOrError<UClass*, EPropertyBagResult> Result = Value.GetValueClass(PropertyBagDefaultPropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<UClass*>(Result, OutValue);
}

FString UMovieGraphValueContainer::GetValueSerializedString()
{
	TValueOrError<FString, EPropertyBagResult> Result = Value.GetValueSerializedString(PropertyBagDefaultPropertyName);
	FString ResultString;
	UE::MovieGraph::Private::GetOptionalValue<FString>(Result, ResultString);
	return ResultString;
}

bool UMovieGraphValueContainer::SetValueBool(const bool bInValue)
{
	return Value.SetValueBool(PropertyBagDefaultPropertyName, bInValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueByte(const uint8 InValue)
{
	return Value.SetValueByte(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueInt32(const int32 InValue)
{
	return Value.SetValueInt32(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueInt64(const int64 InValue)
{
	return Value.SetValueInt64(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueFloat(const float InValue)
{
	return Value.SetValueFloat(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueDouble(const double InValue)
{
	return Value.SetValueDouble(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueName(const FName InValue)
{
	return Value.SetValueName(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueString(const FString& InValue)
{
	return Value.SetValueString(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueText(const FText& InValue)
{
	return Value.SetValueText(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueEnum(const uint8 InValue, const UEnum* Enum)
{
	return Value.SetValueEnum(PropertyBagDefaultPropertyName, InValue, Enum) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueStruct(FConstStructView InValue)
{
	return Value.SetValueStruct(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueObject(UObject* InValue)
{
	return Value.SetValueObject(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueClass(UClass* InValue)
{
	return Value.SetValueClass(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueSerializedString(const FString& NewValue)
{
	return Value.SetValueSerializedString(PropertyBagDefaultPropertyName, NewValue) == EPropertyBagResult::Success;
}

EMovieGraphValueType UMovieGraphValueContainer::GetValueType() const
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyBagDefaultPropertyName))
	{
		return static_cast<EMovieGraphValueType>(Desc->ValueType);
	}

	return EMovieGraphValueType::None;
}

void UMovieGraphValueContainer::SetValueType(EMovieGraphValueType ValueType)
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyBagDefaultPropertyName))
	{
		FPropertyBagPropertyDesc NewDesc(*Desc);
		NewDesc.ValueType = static_cast<EPropertyBagPropertyType>(ValueType);

		const UPropertyBag* NewPropBag = UPropertyBag::GetOrCreateFromDescs({NewDesc});
		Value.MigrateToNewBagStruct(NewPropBag);
	}
}

const UObject* UMovieGraphValueContainer::GetValueTypeObject() const
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyBagDefaultPropertyName))
	{
		return Desc->ValueTypeObject;
	}
	
	return nullptr;
}

void UMovieGraphValueContainer::SetValueTypeObject(const UObject* ValueTypeObject)
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyBagDefaultPropertyName))
	{
		FPropertyBagPropertyDesc NewDesc(*Desc);
		NewDesc.ValueTypeObject = ValueTypeObject;

		const UPropertyBag* NewPropBag = UPropertyBag::GetOrCreateFromDescs({NewDesc});
		Value.MigrateToNewBagStruct(NewPropBag);
	}
}

EMovieGraphContainerType UMovieGraphValueContainer::GetValueContainerType() const
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyBagDefaultPropertyName))
	{
		return static_cast<EMovieGraphContainerType>(Desc->ContainerTypes.GetFirstContainerType());
	}

	return EMovieGraphContainerType::None;
}

void UMovieGraphValueContainer::SetValueContainerType(EMovieGraphContainerType ContainerType)
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyBagDefaultPropertyName))
	{
		FPropertyBagPropertyDesc NewDesc(*Desc);
		NewDesc.ContainerTypes = { static_cast<EPropertyBagContainerType>(ContainerType) };

		const UPropertyBag* NewPropBag = UPropertyBag::GetOrCreateFromDescs({NewDesc});
		Value.MigrateToNewBagStruct(NewPropBag);
	}
}