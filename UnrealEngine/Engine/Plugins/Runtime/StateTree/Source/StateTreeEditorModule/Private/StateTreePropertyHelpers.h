// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PropertyEditorModule.h"
#include "Editor.h"

struct FGuid;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "StateTreeEditor"

namespace UE::StateTree::PropertyHelpers {

/**
 * Gets a struct value from property handle, checks type before access. Expects T is struct.
 * @param ValueProperty Handle to property where value is got from.
 * @return Requested value as optional, in case of multiple values the optional is unset.
 */
template<typename T>
FPropertyAccess::Result GetStructValue(const TSharedPtr<IPropertyHandle>& ValueProperty, T& OutValue)
{
	if (!ValueProperty)
	{
		return FPropertyAccess::Fail;
	}

	FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(ValueProperty->GetProperty());
	check(StructProperty);
	check(StructProperty->Struct == TBaseStructure<T>::Get());

	T Value = T();
	bool bValueSet = false;

	TArray<void*> RawData;
	ValueProperty->AccessRawData(RawData);
	for (void* Data : RawData)
	{
		if (Data)
		{
			const T& CurValue = *reinterpret_cast<T*>(Data);
			if (!bValueSet)
			{
				bValueSet = true;
				Value = CurValue;
			}
			else if (CurValue != Value)
			{
				// Multiple values
				return FPropertyAccess::MultipleValues;
			}
		}
	}

	OutValue = Value;

	return FPropertyAccess::Success;
}

/**
 * Sets a struct property to specific value, checks type before access. Expects T is struct.
 * @param ValueProperty Handle to property where value is got from.
 * @return Requested value as optional, in case of multiple values the optional is unset.
 */
template<typename T>
FPropertyAccess::Result SetStructValue(const TSharedPtr<IPropertyHandle>& ValueProperty, const T& NewValue, EPropertyValueSetFlags::Type Flags = 0)
{
	if (!ValueProperty)
	{
		return FPropertyAccess::Fail;
	}

	FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(ValueProperty->GetProperty());
	if (!StructProperty)
	{
		return FPropertyAccess::Fail;
	}
	if (StructProperty->Struct != TBaseStructure<T>::Get())
	{
		return FPropertyAccess::Fail;
	}

	const bool bTransactable = (Flags & EPropertyValueSetFlags::NotTransactable) == 0;
	bool bNotifiedPreChange = false;
	TArray<void*> RawData;
	ValueProperty->AccessRawData(RawData);
	for (void* Data : RawData)
	{
		if (Data)
		{
			if (!bNotifiedPreChange)
			{
				if (bTransactable && GEditor)
				{
					GEditor->BeginTransaction(FText::Format(LOCTEXT("SetPropertyValue", "Set {0}"), ValueProperty->GetPropertyDisplayName()));
				}
				ValueProperty->NotifyPreChange();
				bNotifiedPreChange = true;
			}

			T* Value = reinterpret_cast<T*>(Data);
			*Value = NewValue;
		}
	}

	if (bNotifiedPreChange)
	{
		ValueProperty->NotifyPostChange(EPropertyChangeType::ValueSet);
		if (bTransactable && GEditor)
		{
			GEditor->EndTransaction();
		}
	}

	ValueProperty->NotifyFinishedChangingProperties();

	return FPropertyAccess::Success;
}

} // UE::StateTree::PropertyHelpers

#undef LOCTEXT_NAMESPACE