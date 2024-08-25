// Copyright Epic Games, Inc. All Rights Reserved.

#include "MovieGraphValueContainer.h"

#include "MovieGraphCommon.h"

// The property bag has one "default" property in it
const FName UMovieGraphValueContainer::PropertyBagDefaultPropertyName("Value");

UMovieGraphValueContainer::UMovieGraphValueContainer()
{
	PropertyName = PropertyBagDefaultPropertyName;
	
	if (!HasAnyFlags(RF_ClassDefaultObject))
	{
		// Add a default double property if one does not exist already
		if (Value.GetNumPropertiesInBag() == 0)
		{
			Value.AddProperty(PropertyName, EPropertyBagPropertyType::Double);
		}
	}
}

void UMovieGraphValueContainer::SetPropertyName(const FName& InName)
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyName))
	{
		if (Desc->Name == InName)
		{
			return;
		}

		Modify();
		PropertyName = InName;

		// Changing the property name requires a new desc and a migration
		FPropertyBagPropertyDesc NewDesc(*Desc);
		NewDesc.Name = InName;

		const UPropertyBag* NewPropBag = UPropertyBag::GetOrCreateFromDescs({NewDesc});
		Value.MigrateToNewBagStruct(NewPropBag);
	}
}

FName UMovieGraphValueContainer::GetPropertyName() const
{
	return PropertyName;
}

bool UMovieGraphValueContainer::GetValueBool(bool& bOutValue) const
{
	TValueOrError<bool, EPropertyBagResult> Result = Value.GetValueBool(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<bool>(Result, bOutValue);
}

bool UMovieGraphValueContainer::GetValueByte(uint8& OutValue) const
{
	TValueOrError<uint8, EPropertyBagResult> Result = Value.GetValueByte(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<uint8>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueInt32(int32& OutValue) const
{
	TValueOrError<int32, EPropertyBagResult> Result = Value.GetValueInt32(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<int32>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueInt64(int64& OutValue) const
{
	TValueOrError<int64, EPropertyBagResult> Result = Value.GetValueInt64(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<int64>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueFloat(float& OutValue) const
{
	TValueOrError<float, EPropertyBagResult> Result = Value.GetValueFloat(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<float>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueDouble(double& OutValue) const
{
	TValueOrError<double, EPropertyBagResult> Result = Value.GetValueDouble(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<double>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueName(FName& OutValue) const
{
	TValueOrError<FName, EPropertyBagResult> Result = Value.GetValueName(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<FName>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueString(FString& OutValue) const
{
	TValueOrError<FString, EPropertyBagResult> Result = Value.GetValueString(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<FString>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueText(FText& OutValue) const
{
	TValueOrError<FText, EPropertyBagResult> Result = Value.GetValueText(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<FText>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueEnum(uint8& OutValue, const UEnum* RequestedEnum) const
{
	TValueOrError<uint8, EPropertyBagResult> Result = Value.GetValueEnum(PropertyName, RequestedEnum);
	return UE::MovieGraph::Private::GetOptionalValue<uint8>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueStruct(FStructView& OutValue, const UScriptStruct* RequestedStruct) const
{
	TValueOrError<FStructView, EPropertyBagResult> Result = Value.GetValueStruct(PropertyName, RequestedStruct);
	return UE::MovieGraph::Private::GetOptionalValue<FStructView>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueObject(UObject* OutValue, const UClass* RequestedClass) const
{
	TValueOrError<UObject*, EPropertyBagResult> Result = Value.GetValueObject(PropertyName, RequestedClass);
	return UE::MovieGraph::Private::GetOptionalValue<UObject*>(Result, OutValue);
}

bool UMovieGraphValueContainer::GetValueClass(UClass*& OutValue) const
{
	TValueOrError<UClass*, EPropertyBagResult> Result = Value.GetValueClass(PropertyName);
	return UE::MovieGraph::Private::GetOptionalValue<UClass*>(Result, OutValue);
}

FString UMovieGraphValueContainer::GetValueSerializedString()
{
	TValueOrError<FString, EPropertyBagResult> Result = Value.GetValueSerializedString(PropertyName);
	FString ResultString;
	UE::MovieGraph::Private::GetOptionalValue<FString>(Result, ResultString);
	return ResultString;
}

bool UMovieGraphValueContainer::SetValueBool(const bool bInValue)
{
	Modify();
	return Value.SetValueBool(PropertyName, bInValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueByte(const uint8 InValue)
{
	Modify();
	return Value.SetValueByte(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueInt32(const int32 InValue)
{
	Modify();
	return Value.SetValueInt32(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueInt64(const int64 InValue)
{
	Modify();
	return Value.SetValueInt64(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueFloat(const float InValue)
{
	Modify();
	return Value.SetValueFloat(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueDouble(const double InValue)
{
	Modify();
	return Value.SetValueDouble(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueName(const FName InValue)
{
	Modify();
	return Value.SetValueName(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueString(const FString& InValue)
{
	Modify();
	return Value.SetValueString(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueText(const FText& InValue)
{
	Modify();
	return Value.SetValueText(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueEnum(const uint8 InValue, const UEnum* Enum)
{
	Modify();
	return Value.SetValueEnum(PropertyName, InValue, Enum) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueStruct(FConstStructView InValue)
{
	Modify();
	return Value.SetValueStruct(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueObject(UObject* InValue)
{
	Modify();
	return Value.SetValueObject(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueClass(UClass* InValue)
{
	Modify();
	return Value.SetValueClass(PropertyName, InValue) == EPropertyBagResult::Success;
}

bool UMovieGraphValueContainer::SetValueSerializedString(const FString& NewValue)
{
	Modify();
	return Value.SetValueSerializedString(PropertyName, NewValue) == EPropertyBagResult::Success;
}

EMovieGraphValueType UMovieGraphValueContainer::GetValueType() const
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyName))
	{
		return static_cast<EMovieGraphValueType>(Desc->ValueType);
	}

	return EMovieGraphValueType::None;
}

void UMovieGraphValueContainer::SetValueType(EMovieGraphValueType ValueType, UObject* InValueTypeObject)
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyName))
	{
		Modify();

		FPropertyBagPropertyDesc NewDesc;
		NewDesc.Name = Desc->Name;
		NewDesc.ValueType = static_cast<EPropertyBagPropertyType>(ValueType);
		NewDesc.ContainerTypes = Desc->ContainerTypes;
		NewDesc.ValueTypeObject = InValueTypeObject;

		Value.Reset();
		Value.AddProperties({NewDesc});

#if WITH_EDITOR
		// Send a property change event manually since the property bag doesn't seem to generate one in this scenario
		FProperty* ValueProperty = FindFProperty<FProperty>(StaticClass(), GET_MEMBER_NAME_CHECKED(UMovieGraphValueContainer, Value));
		FPropertyChangedEvent PropertyEvent(ValueProperty, EPropertyChangeType::ValueSet);
		PostEditChangeProperty(PropertyEvent);
#endif // WITH_EDITOR
	}
}

const UObject* UMovieGraphValueContainer::GetValueTypeObject() const
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyName))
	{
		return Desc->ValueTypeObject;
	}
	
	return nullptr;
}

void UMovieGraphValueContainer::SetValueTypeObject(const UObject* ValueTypeObject)
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyName))
	{
		Modify();

		FPropertyBagPropertyDesc NewDesc(*Desc);
		NewDesc.ValueTypeObject = ValueTypeObject;

		const UPropertyBag* NewPropBag = UPropertyBag::GetOrCreateFromDescs({NewDesc});
		Value.MigrateToNewBagStruct(NewPropBag);
	}
}

EMovieGraphContainerType UMovieGraphValueContainer::GetValueContainerType() const
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyName))
	{
		return static_cast<EMovieGraphContainerType>(Desc->ContainerTypes.GetFirstContainerType());
	}

	return EMovieGraphContainerType::None;
}

void UMovieGraphValueContainer::SetValueContainerType(EMovieGraphContainerType ContainerType)
{
	if (const FPropertyBagPropertyDesc* Desc = Value.FindPropertyDescByName(PropertyName))
	{
		Modify();

		FPropertyBagPropertyDesc NewDesc(*Desc);
		NewDesc.ContainerTypes = { static_cast<EPropertyBagContainerType>(ContainerType) };

		const UPropertyBag* NewPropBag = UPropertyBag::GetOrCreateFromDescs({NewDesc});
		Value.MigrateToNewBagStruct(NewPropBag);
	}
}

TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> UMovieGraphValueContainer::GetArrayRef()
{
	return Value.GetMutableArrayRef(PropertyName);
}

void UMovieGraphValueContainer::SetFromDesc(const FPropertyBagPropertyDesc* InDesc, const FString& InString)
{
	if (InDesc)
	{
		Modify();

		FPropertyBagPropertyDesc NewDesc;
		NewDesc.Name = InDesc->Name;
		NewDesc.ValueType = InDesc->ValueType;
		NewDesc.ContainerTypes = InDesc->ContainerTypes;
		NewDesc.ValueTypeObject = InDesc->ValueTypeObject;

		PropertyName = InDesc->Name;
		
		Value.Reset();
		Value.AddProperties({NewDesc});

		SetValueSerializedString(InString);
	}
}
