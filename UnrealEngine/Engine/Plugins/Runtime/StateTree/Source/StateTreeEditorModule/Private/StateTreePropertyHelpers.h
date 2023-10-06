// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Editor.h"
#include "PropertyHandle.h"
#include "Misc/EnumerateRange.h"

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

/**
 * Helper class to deal with relative property paths in PostEditChangeChainProperty().
 */
struct FStateTreeEditPropertyPath
{
private:
	struct FStateTreeEditPropertySegment
	{
		FStateTreeEditPropertySegment() = default;
		FStateTreeEditPropertySegment(const FProperty* InProperty, const FName InPropertyName, const int32 InArrayIndex = INDEX_NONE)
			: Property(InProperty)
			, PropertyName(InPropertyName)
			, ArrayIndex(InArrayIndex)
		{
		}
	
		const FProperty* Property = nullptr;
		FName PropertyName = FName();
		int32 ArrayIndex = INDEX_NONE;
	};
	
public:
	FStateTreeEditPropertyPath() = default;

	/** Makes property path relative to BaseStruct. Checks if the path is not part of the type. */
	explicit FStateTreeEditPropertyPath(const UStruct* BaseStruct, const FString& InPath)
	{
		TArray<FString> PathSegments;
		InPath.ParseIntoArray(PathSegments, TEXT("."));

		const UStruct* CurrBase = BaseStruct;
		for (const FString& Segment : PathSegments)
		{
			const FName PropertyName(Segment);
			if (const FProperty* Property = CurrBase->FindPropertyByName(PropertyName))
			{
				Path.Emplace(Property, PropertyName);

				if (const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
				{
					Property = ArrayProperty->Inner;
				}

				if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
				{
					CurrBase = StructProperty->Struct;
				}
				else if (const FObjectProperty* ObjectProperty = CastField<FObjectProperty>(Property))
				{
					CurrBase = ObjectProperty->PropertyClass;
				}
			}
			else
			{
				checkf(TEXT("Path %s id not part of type %s."), *InPath, *GetNameSafe(BaseStruct));
				Path.Reset();
				break;
			}
		}
	}

	/** Makes property path from property change event. */
	explicit FStateTreeEditPropertyPath(const FPropertyChangedChainEvent& PropertyChangedEvent)
	{
		FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = PropertyChangedEvent.PropertyChain.GetActiveMemberNode();
		while (PropertyNode != nullptr)
		{
			if (FProperty* Property = PropertyNode->GetValue())
			{
				const FName PropertyName = Property->GetFName(); 
				const int32 ArrayIndex = PropertyChangedEvent.GetArrayIndex(PropertyName.ToString());
				Path.Emplace(Property, PropertyName, ArrayIndex);
			}
			PropertyNode = PropertyNode->GetNextNode();
		}
	}

	/** @return true if the property path contains specified path. */
	bool ContainsPath(const FStateTreeEditPropertyPath& InPath) const
	{
		if (InPath.Path.Num() > Path.Num())
    	{
    		return false;
    	}

    	for (TConstEnumerateRef<FStateTreeEditPropertySegment> Segment : EnumerateRange(InPath.Path))
    	{
    		if (Segment->PropertyName != Path[Segment.GetIndex()].PropertyName)
    		{
    			return false;
    		}
    	}
    	return true;
	}

	/** @return true if the property path is exactly the specified path. */
	bool IsPathExact(const FStateTreeEditPropertyPath& InPath) const
	{
		if (InPath.Path.Num() != Path.Num())
		{
			return false;
		}

		for (TConstEnumerateRef<FStateTreeEditPropertySegment> Segment : EnumerateRange(InPath.Path))
		{
			if (Segment->PropertyName != Path[Segment.GetIndex()].PropertyName)
			{
				return false;
			}
		}
		return true;
	}

	/** @return array index at specified property, or INDEX_NONE, if the property is not array or property not found.  */
	int32 GetPropertyArrayIndex(const FStateTreeEditPropertyPath& InPath) const
	{
		return ContainsPath(InPath) ? Path[InPath.Path.Num() - 1].ArrayIndex : INDEX_NONE;
	}

private:
	TArray<FStateTreeEditPropertySegment> Path;
};

#undef LOCTEXT_NAMESPACE
