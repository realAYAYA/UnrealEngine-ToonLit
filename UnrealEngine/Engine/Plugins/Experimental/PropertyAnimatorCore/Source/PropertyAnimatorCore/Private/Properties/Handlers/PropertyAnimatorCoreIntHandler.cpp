// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/Handlers/PropertyAnimatorCoreIntHandler.h"

bool UPropertyAnimatorCoreIntHandler::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	if (InPropertyData.IsA<FIntProperty>())
	{
		return true;
	}

	return Super::IsPropertySupported(InPropertyData);
}

bool UPropertyAnimatorCoreIntHandler::GetValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	OutValue.AddProperty(PropertyName, EPropertyBagPropertyType::Int32);

	int32 Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	OutValue.SetValueInt32(PropertyName, Value);

	return true;
}

bool UPropertyAnimatorCoreIntHandler::SetValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	TValueOrError<int32, EPropertyBagResult> ValueResult = InValue.GetValueInt32(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	int32 NewValue = ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreIntHandler::IsAdditiveSupported() const
{
	return true;
}

bool UPropertyAnimatorCoreIntHandler::AddValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	const TValueOrError<int32, EPropertyBagResult> ValueResult = InValue.GetValueInt32(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	int32 Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	int32 NewValue = Value + ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreIntHandler::SubtractValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	const TValueOrError<int32, EPropertyBagResult> ValueResult = InValue.GetValueInt32(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	int32 Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	int32 NewValue = Value - ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}
