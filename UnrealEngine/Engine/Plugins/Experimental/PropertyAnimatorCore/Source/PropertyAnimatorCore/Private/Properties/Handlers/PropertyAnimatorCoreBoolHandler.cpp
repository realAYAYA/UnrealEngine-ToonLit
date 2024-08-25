// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/Handlers/PropertyAnimatorCoreBoolHandler.h"

bool UPropertyAnimatorCoreBoolHandler::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	if (InPropertyData.IsA<FBoolProperty>())
	{
		return true;
	}

	return Super::IsPropertySupported(InPropertyData);
}

bool UPropertyAnimatorCoreBoolHandler::GetValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	OutValue.AddProperty(PropertyName, EPropertyBagPropertyType::Bool);

	bool bValue;
	InPropertyData.GetPropertyValuePtr(&bValue);

	OutValue.SetValueBool(PropertyName, bValue);

	return true;
}

bool UPropertyAnimatorCoreBoolHandler::SetValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	TValueOrError<bool, EPropertyBagResult> ValueResult = InValue.GetValueBool(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	bool bNewValue = ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&bNewValue);

	return true;
}