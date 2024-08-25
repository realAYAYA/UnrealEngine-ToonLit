// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/Handlers/PropertyAnimatorCoreDoubleHandler.h"

bool UPropertyAnimatorCoreDoubleHandler::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	if (InPropertyData.IsA<FDoubleProperty>())
	{
		return true;
	}

	return Super::IsPropertySupported(InPropertyData);
}

bool UPropertyAnimatorCoreDoubleHandler::GetValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	OutValue.AddProperty(PropertyName, EPropertyBagPropertyType::Double);

	double Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	OutValue.SetValueDouble(PropertyName, Value);

	return true;
}

bool UPropertyAnimatorCoreDoubleHandler::SetValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	TValueOrError<double, EPropertyBagResult> ValueResult = InValue.GetValueDouble(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	double NewValue = ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreDoubleHandler::IsAdditiveSupported() const
{
	return true;
}

bool UPropertyAnimatorCoreDoubleHandler::AddValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	const TValueOrError<double, EPropertyBagResult> ValueResult = InValue.GetValueDouble(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	double Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	double NewValue = Value + ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreDoubleHandler::SubtractValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	const TValueOrError<double, EPropertyBagResult> ValueResult = InValue.GetValueDouble(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	double Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	double NewValue = Value - ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}
