// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/Handlers/PropertyAnimatorCoreFloatHandler.h"

bool UPropertyAnimatorCoreFloatHandler::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	if (InPropertyData.IsA<FFloatProperty>())
	{
		return true;
	}

	return Super::IsPropertySupported(InPropertyData);
}

bool UPropertyAnimatorCoreFloatHandler::GetValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	OutValue.AddProperty(PropertyName, EPropertyBagPropertyType::Float);

	float Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	OutValue.SetValueFloat(PropertyName, Value);

	return true;
}

bool UPropertyAnimatorCoreFloatHandler::SetValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	TValueOrError<float, EPropertyBagResult> ValueResult = InValue.GetValueFloat(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	float& NewValue = ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreFloatHandler::IsAdditiveSupported() const
{
	return true;
}

bool UPropertyAnimatorCoreFloatHandler::AddValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	const TValueOrError<float, EPropertyBagResult> ValueResult = InValue.GetValueFloat(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	float Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	float NewValue = Value + ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreFloatHandler::SubtractValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	const TValueOrError<float, EPropertyBagResult> ValueResult = InValue.GetValueFloat(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	float Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	float NewValue = Value - ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}
