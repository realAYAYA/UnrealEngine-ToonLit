// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Editor.h"

class IPropertyHandle;

#define LOCTEXT_NAMESPACE "MassMovementEditor"

namespace UE::MassMovement::PropertyUtils
{

// Expects T is struct.
template<typename T>
TOptional<T> GetValue(const TSharedPtr<IPropertyHandle>& ValueProperty)
{
	if (!ValueProperty)
	{
		return TOptional<T>();
	}

	FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(ValueProperty->GetProperty());
	if (!StructProperty)
	{
		return TOptional<T>();
	}
	if (StructProperty->Struct != TBaseStructure<T>::Get())
	{
		return TOptional<T>();
	}

	T Value = T();
	bool bValueSet = false;

	TArray<void*> RawData;
	ValueProperty->AccessRawData(RawData);
	for (void* Data : RawData)
	{
		if (Data)
		{
			T CurValue = *reinterpret_cast<T*>(Data);
			if (!bValueSet)
			{
				bValueSet = true;
				Value = CurValue;
			}
			else if (CurValue != Value)
			{
				// Multiple values
				return TOptional<T>();
			}
		}
	}

	return TOptional<T>(Value);
}

// Expects T is struct.
template<typename T>
void SetValue(TSharedPtr<IPropertyHandle>& ValueProperty, T NewValue, EPropertyValueSetFlags::Type Flags = 0)
{
	if (!ValueProperty)
	{
		return;
	}

	FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(ValueProperty->GetProperty());
	if (!StructProperty)
	{
		return;
	}
	if (StructProperty->Struct != TBaseStructure<T>::Get())
	{
		return;
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
}

} // UE::MassMovement::PropertyUtils

#undef LOCTEXT_NAMESPACE