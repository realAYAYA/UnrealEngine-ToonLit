// Copyright Epic Games, Inc. All Rights Reserved.

#include "KeyPropertyParams.h"

#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "Misc/AssertionMacros.h"
#include "PropertyHandle.h"
#include "Templates/SharedPointer.h"
#include "UObject/Class.h"
#include "UObject/Object.h"

FPropertyPath PropertyHandleToPropertyPath(const UClass* OwnerClass, const IPropertyHandle& InPropertyHandle)
{
	TArray<FPropertyInfo> PropertyInfo;
	PropertyInfo.Insert(FPropertyInfo(InPropertyHandle.GetProperty(), InPropertyHandle.GetIndexInArray()), 0);

	TSharedPtr<IPropertyHandle> CurrentHandle = InPropertyHandle.GetParentHandle();
	while (CurrentHandle.IsValid() && CurrentHandle->GetProperty() != nullptr)
	{
		// IPropertyHandle chains contain arrays in a manner designed for display in the property editor, e.g.
		// Container.Array.Array[ArrayIndex].StructInner.
		// We need to collapse adjacent array properties as we are looking for Container.Array[ArrayIndex].StructInner
		// to for a well-formed 'property path'
		TSharedPtr<IPropertyHandle> ParentHandle = CurrentHandle->GetParentHandle();
		if (ParentHandle.IsValid() && ParentHandle->GetProperty() != nullptr && ParentHandle->GetProperty()->IsA<FArrayProperty>())
		{
			FArrayProperty* ArrayProperty = CastFieldChecked<FArrayProperty>(ParentHandle->GetProperty());

			PropertyInfo.Insert(FPropertyInfo(ParentHandle->GetProperty(), CurrentHandle->GetIndexInArray()), 0);
			CurrentHandle = ParentHandle->GetParentHandle();
		}
		else
		{
			PropertyInfo.Insert(FPropertyInfo(CurrentHandle->GetProperty(), CurrentHandle->GetIndexInArray()), 0);
			CurrentHandle = CurrentHandle->GetParentHandle();
		}
	}

	FPropertyPath PropertyPath;
	for (const FPropertyInfo& Info : PropertyInfo)
	{
		PropertyPath.AddProperty(Info);
	}
	return PropertyPath;
}

FCanKeyPropertyParams::FCanKeyPropertyParams(const UClass* InObjectClass, const FPropertyPath& InPropertyPath)
	: ObjectClass(InObjectClass)
	, PropertyPath(InPropertyPath)
{
}

FCanKeyPropertyParams::FCanKeyPropertyParams(const UClass* InObjectClass, const IPropertyHandle& InPropertyHandle)
	: ObjectClass(InObjectClass)
	, PropertyPath(PropertyHandleToPropertyPath(InObjectClass, InPropertyHandle))
{
}

const UStruct* FCanKeyPropertyParams::FindPropertyOwner(const FProperty* ForProperty) const
{
	check(ForProperty);

	bool bFoundProperty = false;
	for (int32 Index = PropertyPath.GetNumProperties() - 1; Index >= 0; --Index)
	{
		FProperty* Property = PropertyPath.GetPropertyInfo(Index).Property.Get();
		if (!bFoundProperty)
		{
			bFoundProperty = Property == ForProperty;
			if (bFoundProperty)
			{
				return Property->GetOwnerStruct();
			}
		}
		else if (const FStructProperty* StructProperty = CastField<const FStructProperty>(Property))
		{
			return StructProperty->Struct;
		}
	}
	return ObjectClass;
}

const UStruct* FCanKeyPropertyParams::FindPropertyContainer(const FProperty* ForProperty) const
{
	check(ForProperty);

	bool bFoundProperty = false;
	for (int32 Index = PropertyPath.GetNumProperties() - 1; Index >= 0; --Index)
	{
		FProperty* Property = PropertyPath.GetPropertyInfo(Index).Property.Get();
		if (!bFoundProperty)
		{
			bFoundProperty = Property == ForProperty;
			if (bFoundProperty)
			{
				return ObjectClass;
			}
		}
		else if (const FStructProperty* StructProperty = CastField<const FStructProperty>(Property))
		{
			return StructProperty->Struct;
		}
	}
	return ObjectClass;
}

FKeyPropertyParams::FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const FPropertyPath& InPropertyPath, ESequencerKeyMode InKeyMode)
	: ObjectsToKey(InObjectsToKey)
	, PropertyPath(InPropertyPath)
	, KeyMode(InKeyMode)

{
}

FKeyPropertyParams::FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const IPropertyHandle& InPropertyHandle, ESequencerKeyMode InKeyMode)
	: ObjectsToKey(InObjectsToKey)
	, PropertyPath(InObjectsToKey.Num() > 0 ? PropertyHandleToPropertyPath(InObjectsToKey[0]->GetClass(), InPropertyHandle) : FPropertyPath())
	, KeyMode(InKeyMode)
{
}

FPropertyChangedParams::FPropertyChangedParams(TArray<UObject*> InObjectsThatChanged, const FPropertyPath& InPropertyPath, const FPropertyPath& InStructPathToKey, ESequencerKeyMode InKeyMode)
	: ObjectsThatChanged(InObjectsThatChanged)
	, PropertyPath(InPropertyPath)
	, StructPathToKey(InStructPathToKey)
	, KeyMode(InKeyMode)
{
}

template<> bool FPropertyChangedParams::GetPropertyValueImpl<bool>(void* Data, const FPropertyInfo& PropertyInfo)
{
	// Bool property values are stored in a bit field so using a straight cast of the pointer to get their value does not
	// work.  Instead use the actual property to get the correct value.
	if ( const FBoolProperty* BoolProperty = CastField<const FBoolProperty>( PropertyInfo.Property.Get() ) )
	{
		return BoolProperty->GetPropertyValue(Data);
	}
	else
	{
		return *((bool*)Data);
	}
}

template<> UObject* FPropertyChangedParams::GetPropertyValueImpl<UObject*>(void* Data, const FPropertyInfo& PropertyInfo)
{
	if (const FObjectPropertyBase* ObjectProperty = CastField<const FObjectPropertyBase>(PropertyInfo.Property.Get()))
	{
		return ObjectProperty->GetObjectPropertyValue(Data);
	}
	else
	{
		return *((UObject**)Data);
	}
}

FString FPropertyChangedParams::GetPropertyPathString() const
{
	return PropertyPath.ToString(TEXT("."));
}
