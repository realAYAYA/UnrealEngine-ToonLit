// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "Math/UnrealMathSSE.h"
#include "PropertyPath.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakFieldPtr.h"

class IPropertyHandle;
class UClass;
class UObject;
class UStruct;

enum class ESequencerKeyMode;

/**
 * Parameters for determining if a property can be keyed.
 */
struct SEQUENCER_API FCanKeyPropertyParams
{
	/**
	 * Creates new can key property parameters.
	 * @param InObjectClass the class of the object which has the property to be keyed.
	 * @param InPropertyPath path get from the root object to the property to be keyed.
	 */
	FCanKeyPropertyParams(const UClass* InObjectClass, const FPropertyPath& InPropertyPath);

	/**
	* Creates new can key property parameters.
	* @param InObjectClass the class of the object which has the property to be keyed.
	* @param InPropertyHandle a handle to the property to be keyed.
	*/
	FCanKeyPropertyParams(const UClass* InObjectClass, const IPropertyHandle& InPropertyHandle);

	/** The owner struct */
	const UStruct* FindPropertyOwner(const FProperty* ForProperty) const;

	/** The container from which to find setter functions */
	const UStruct* FindPropertyContainer(const FProperty* ForProperty) const;

	/** The class of the object which has the property to be keyed. */
	const UClass* ObjectClass;

	/** A path of properties to get from the root object to the property to be keyed. */
	FPropertyPath PropertyPath;
};

/**
 * Parameters for keying a property.
 */
struct SEQUENCER_API FKeyPropertyParams
{
	/**
	* Creates new key property parameters for a manually triggered property change.
	* @param InObjectsToKey an array of the objects who's property will be keyed.
	* @param InPropertyPath path get from the root object to the property to be keyed.
	*/
	FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const FPropertyPath& InPropertyPath, ESequencerKeyMode InKeyMode);

	/**
	* Creates new key property parameters from an actual property change notification with a property handle.
	* @param InObjectsToKey an array of the objects who's property will be keyed.
	* @param InPropertyHandle a handle to the property to be keyed.
	*/
	FKeyPropertyParams(TArray<UObject*> InObjectsToKey, const IPropertyHandle& InPropertyHandle, ESequencerKeyMode InKeyMode);

	/** An array of the objects who's property will be keyed. */
	const TArray<UObject*> ObjectsToKey;

	/** A path of properties to get from the root object to the property to be keyed. */
	FPropertyPath PropertyPath;

	/** Keyframing params */
	const ESequencerKeyMode KeyMode;
};

/**
 * Parameters for the property changed callback.
 */
class SEQUENCER_API FPropertyChangedParams
{
public:
	FPropertyChangedParams(TArray<UObject*> InObjectsThatChanged, const FPropertyPath& InPropertyPath, const FPropertyPath& InStructPathToKey, ESequencerKeyMode InKeyMode);

	/**
	 * Gets the value of the property that changed.
	 */
	template<typename ValueType>
	ValueType GetPropertyValue() const
	{
		void* ContainerPtr = ObjectsThatChanged[0];
		for (int32 i = 0; i < PropertyPath.GetNumProperties(); i++)
		{
			const FPropertyInfo& PropertyInfo = PropertyPath.GetPropertyInfo(i);
			if (FProperty* Property = PropertyInfo.Property.Get())
			{
				int32 ArrayIndex = FMath::Max(0, PropertyInfo.ArrayIndex);
				if (FArrayProperty* ArrayProp = CastField<FArrayProperty>(Property))
				{
					// Sometimes property paths have the array property twice, first with no array index,
					// then a second so we skip over this property if that's the case
					if (PropertyInfo.ArrayIndex == INDEX_NONE && i < PropertyPath.GetNumProperties()-1)
					{
						const FPropertyInfo& InnerPropertyInfo = PropertyPath.GetPropertyInfo(i+1);
						FProperty* InnerProperty = InnerPropertyInfo.Property.Get();
						if (InnerProperty && InnerProperty->GetOwner<FProperty>() == ArrayProp)
						{
							ArrayIndex = InnerPropertyInfo.ArrayIndex;
							++i;
						}
					}

					FScriptArrayHelper ParentArrayHelper(ArrayProp, ArrayProp->ContainerPtrToValuePtr<void>(ContainerPtr));
					if (!ParentArrayHelper.IsValidIndex(ArrayIndex))
					{
						return ValueType();
					}
					ContainerPtr = ParentArrayHelper.GetRawPtr(ArrayIndex);
				}
				else if (ArrayIndex >= 0 && ArrayIndex < Property->ArrayDim)
				{
					ContainerPtr = Property->ContainerPtrToValuePtr<ValueType>(ContainerPtr, ArrayIndex);
				}
			}
		}

		return GetPropertyValueImpl<ValueType>(ContainerPtr, PropertyPath.GetLeafMostProperty());
	}

	/** Gets the property path as a period seperated string of property names. */
	FString GetPropertyPathString() const;

	/** An array of the objects that changed. */
	const TArray<UObject*> ObjectsThatChanged;

	/** A path of properties to get from the root object to the property to be keyed. */
	FPropertyPath PropertyPath;

	/** Represents the path of an inner property which should be keyed for a struct property.  If all inner 
	properties should be keyed, this will be empty. */
	FPropertyPath StructPathToKey;

	/** Keyframing params */
	const ESequencerKeyMode KeyMode;

private:

	template<typename ValueType>
	static ValueType GetPropertyValueImpl(void* Data, const FPropertyInfo& Info)
	{
		return *((ValueType*)Data);
	}
};

template<> SEQUENCER_API bool FPropertyChangedParams::GetPropertyValueImpl<bool>(void* Data, const FPropertyInfo& Info);
template<> SEQUENCER_API UObject* FPropertyChangedParams::GetPropertyValueImpl<UObject*>(void* Data, const FPropertyInfo& Info);