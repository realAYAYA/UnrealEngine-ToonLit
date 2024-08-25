// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Metadata/PCGAttributePropertySelector.h"
#include "Metadata/PCGMetadataAttributeTpl.h"
#include "Metadata/Accessors/IPCGAttributeAccessor.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#include "UObject/UnrealType.h" // IWYU pragma: keep

enum class EPCGMetadataTypes : uint8;

struct FPCGContext;
class UPCGData;
class UPCGParamData;

namespace PCGPropertyHelpers
{
	/**
	* Get a property value and pass it as a parameter to a callback function.
	* @param InObject - The object to read from
	* @param InProperty - The property to look for
	* @param InFunc - Callback function that can return anything, and should have a single templated argument, where the property will be.
	* @returns Forward the result of the callback.
	*/
	template <typename ObjectType, typename Func>
	decltype(auto) GetPropertyValueWithCallback(const ObjectType* InObject, const FProperty* InProperty, Func InFunc);

	/**
	* Set a property value given by a callback function.
	* @param InObject - The object to write to
	* @param InProperty - The property to look for
	* @param InFunc - Callback function that take a reference to a templated type. It will set the property with this value. returns true if we should set, false otherwise.
	* @returns Forward the result of the callback
	*/
	template <typename ObjectType, typename Func>
	bool SetPropertyValueFromCallback(ObjectType* InObject, const FProperty* InProperty, Func InFunc);

	/**
	* Conversion between property type and PCG type.
	* @param InProperty - The property to look for
	* @returns PCG type if the property is supported, Unknown otherwise.
	*/
	PCG_API EPCGMetadataTypes GetMetadataTypeFromProperty(const FProperty* InProperty);

	struct FExtractorParameters
	{
		// Pointer to the container containing the data we want to extract
		const void* Container = nullptr;

		// Class or ScriptStruct of the object/struct we want to extract the property from.
		const UStruct* Class = nullptr;

		// Selector of the property we want to extract
		FPCGAttributePropertySelector PropertySelector;

		// Optional name of the attribute that will receive the extracted property. If None, will take the property name. 
		// Also not used for Structs/Object extraction, as we will create multiple attributes, and they will be the name of all the extracted members.
		FName OutputAttributeName = NAME_None;

		// If the property we want to extract is an object/struct, give the possibility to extract all their member in one go.
		// Only work if their members are not structs or containers (like array).
		bool bShouldExtract = false;

		// Extra flag if the property needs to be visible to be extractable.
		bool bPropertyNeedsToBeVisible = false;
	};

	/**
	* Extract a given property in an Attribute Set.
	* @param Parameters - Parameters for extraction, cf above.
	* @param OptionalContext - Optional context if the extraction is done in a PCG Node, so errors are using the context to log.
	* @param OptionalObjectTraversed - Optional set to store all objects that we traversed, to be able to react to those objects changes.
	*/
	PCG_API UPCGParamData* ExtractPropertyAsAttributeSet(const FExtractorParameters& Parameters, FPCGContext* OptionalContext = nullptr, TSet<FSoftObjectPath>* OptionalObjectTraversed = nullptr);
}

//////
/// PCGPropertiesHelpers Implementation
//////

// Func signature : auto(auto&&)
template <typename ObjectType, typename Func>
inline decltype(auto) PCGPropertyHelpers::GetPropertyValueWithCallback(const ObjectType* InObject, const FProperty* InProperty, Func InFunc)
{
	if (!InObject || !InProperty)
	{
		return false;
	}

	TUniquePtr<IPCGAttributeAccessor> PropertyAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(InProperty);

	if (!PropertyAccessor.IsValid())
	{
		return false;
	}

	auto Getter = [&PropertyAccessor, &InFunc, InObject](auto Dummy)
	{
		using Type = decltype(Dummy);
		Type Value{};
		FPCGAttributeAccessorKeysSingleObjectPtr<ObjectType> Key(InObject);

		if (PropertyAccessor->Get<Type>(Value, Key))
		{
			return InFunc(Value);
		}
		else
		{
			using ReturnType = decltype(InFunc(0.0));
			if constexpr (std::is_same_v<ReturnType, void>)
			{
				return;
			}
			else
			{
				return ReturnType{};
			}
		}
	};

	return PCGMetadataAttribute::CallbackWithRightType(PropertyAccessor->GetUnderlyingType(), Getter);
}

// Func signature : bool(auto&)
// Will have property value in first arg, and boolean return if we should set the property after.
// Returns true if the set succeeded
template <typename ObjectType, typename Func>
inline bool PCGPropertyHelpers::SetPropertyValueFromCallback(ObjectType* InObject, const FProperty* InProperty, Func InFunc)
{
	if (!InObject || !InProperty)
	{
		return false;
	}

	TUniquePtr<IPCGAttributeAccessor> PropertyAccessor = PCGAttributeAccessorHelpers::CreatePropertyAccessor(InProperty);

	if (!PropertyAccessor.IsValid())
	{
		return false;
	}

	auto Setter = [&PropertyAccessor, &InFunc, InObject](auto Dummy) -> bool
	{
		using Type = decltype(Dummy);
		Type Value{};
		FPCGAttributeAccessorKeysSingleObjectPtr<ObjectType> Key(InObject);

		if (InFunc(Value))
		{
			return PropertyAccessor->Set<Type>(Value, Key);
		}
		else
		{
			return false;
		}
	};

	return PCGMetadataAttribute::CallbackWithRightType(PropertyAccessor->GetUnderlyingType(), Setter);
}
