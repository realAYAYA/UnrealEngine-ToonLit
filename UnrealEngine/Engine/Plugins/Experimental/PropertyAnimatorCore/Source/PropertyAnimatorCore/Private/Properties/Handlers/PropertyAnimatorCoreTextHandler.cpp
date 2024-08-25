// Copyright Epic Games, Inc. All Rights Reserved.

#include "Properties/Handlers/PropertyAnimatorCoreTextHandler.h"
#include "PropertyBag.h"
#include "UObject/TextProperty.h"

bool UPropertyAnimatorCoreTextHandler::IsPropertySupported(const FPropertyAnimatorCoreData& InPropertyData) const
{
	if (InPropertyData.IsA<FTextProperty>())
	{
		return true;
	}

	return Super::IsPropertySupported(InPropertyData);
}

bool UPropertyAnimatorCoreTextHandler::GetValue(const FPropertyAnimatorCoreData& InPropertyData, FInstancedPropertyBag& OutValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	OutValue.AddProperty(PropertyName, EPropertyBagPropertyType::Text);

	FText Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	OutValue.SetValueText(PropertyName, Value);

	return true;
}

bool UPropertyAnimatorCoreTextHandler::SetValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	TValueOrError<FText, EPropertyBagResult> ValueResult = InValue.GetValueText(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	FText NewValue = ValueResult.GetValue();
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreTextHandler::IsAdditiveSupported() const
{
	return true;
}

bool UPropertyAnimatorCoreTextHandler::AddValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	const TValueOrError<FText, EPropertyBagResult> ValueResult = InValue.GetValueText(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	FText Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	// Append
	FText NewValue = FText::FromString(Value.ToString() + ValueResult.GetValue().ToString());
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}

bool UPropertyAnimatorCoreTextHandler::SubtractValue(const FPropertyAnimatorCoreData& InPropertyData, const FInstancedPropertyBag& InValue)
{
	const FName PropertyName(InPropertyData.GetPathHash());
	const TValueOrError<FText, EPropertyBagResult> ValueResult = InValue.GetValueText(PropertyName);
	if (!ValueResult.HasValue())
	{
		return false;
	}

	FText Value;
	InPropertyData.GetPropertyValuePtr(&Value);

	// Trim end
	FString StringValue = Value.ToString();
	const FString StringValueResult = ValueResult.GetValue().ToString();
	if (StringValue.EndsWith(StringValueResult))
	{
		StringValue.LeftChopInline(StringValueResult.Len());
	}

	FText NewValue = FText::FromString(StringValue);
	InPropertyData.SetPropertyValuePtr(&NewValue);

	return true;
}
