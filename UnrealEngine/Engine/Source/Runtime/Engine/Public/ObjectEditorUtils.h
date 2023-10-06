// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/UnrealType.h"

#if WITH_EDITOR
class FField;
class FProperty;

namespace FObjectEditorUtils
{

	/**
	 * Gets the category this field belongs to.
	 *
	 * @param	InField	Field we want the category name of.
	 * @return	Category name of the given field.
	 */
	ENGINE_API FText GetCategoryText( const FField* InField );	

	/**
	 * Gets the category this field belongs to.
	 *
	 * @param	InField	Field we want the category name of.
	 * @return	Category name of the given field.
	 */
	ENGINE_API FText GetCategoryText( const UField* InField );	

	/**
	 * Gets the category this field belongs to.
	 *
	 * @param	InField	Field we want the category name of.
	 * @return	Category name of the given field.
	 */
	ENGINE_API FString GetCategory( const FField* InField );

	/**
	 * Gets the category this field belongs to.
	 *
	 * @param	InField	Field we want the category name of.
	 * @return	Category name of the given field.
	 */
	ENGINE_API FString GetCategory( const UField* InField );

	/**
	 * Gets the FName of the category this field belongs to.  
	 * Note, this value is suitable for comparison against other categories but NOT suitable as a display name since it is not localized
	 *
	 * @param	InField	Field we want the category name of.
	 * @return	Category name of the given field.
	 */
	ENGINE_API FName GetCategoryFName( const FField* InField );

	/**
	 * Gets the FName of the category this field belongs to.  
	 * Note, this value is suitable for comparison against other categories but NOT suitable as a display name since it is not localized
	 *
	 * @param	InField	Field we want the category name of.
	 * @return	Category name of the given field.
	 */
	ENGINE_API FName GetCategoryFName( const UField* InField );

	/**
	 * Query if a function is flagged as hidden from the given class either by category or by function name
	 *
	 * @param	InFunction	Function to check
	 * @param	InClass		Class to check hidden if the function is hidden in
	 *
	 * @return	true if the function is hidden in the specified class.
	 */
	ENGINE_API bool IsFunctionHiddenFromClass( const UFunction* InFunction, const UClass* Class );

	/**
	 * Query if a the category a variable belongs to is flagged as hidden from the given class.
	 *
	 * @param	InVariable	Property to check
	 * @param	InClass		Class to check hidden if the function is hidden in
	 *
	 * @return	true if the functions category is hidden in the specified class.
	 */
	ENGINE_API bool IsVariableCategoryHiddenFromClass( const FProperty* InVariable, const UClass* Class );

	/**
	 * Get the classes development status and return if it's either experimental or early access.
	 * 
	 * @param Class the class to inspect.
	 * @param bIsExperimental [out] value indicating if the class is experimental.
	 * @param bIsEarlyAccess [out] value indicating if the class is early access.
	 * @param MostDerivedClassName [out] The name of the most derived class that is marked as experimental or early access (or empty string)
	 */
	ENGINE_API void GetClassDevelopmentStatus(UClass* Class, bool& bIsExperimental, bool& bIsEarlyAccess, FString& MostDerivedClassName);

	/**
	 * Copy the value of a property from source object to a destination object.
	 *
	 * @param	SourceObject		The object to copy the property value from.
	 * @param	SourceProperty		The property on the SourceObject to copy the value from.
	 * @param	DestinationObject	The object to copy the property value to.
	 * @param	DestinationProperty	The property on the DestinationObject to copy the value to.
	 *
	 * @return true if the value was successfully migrated.
	 */
	ENGINE_API bool MigratePropertyValue(UObject* SourceObject, FProperty* SourceProperty, UObject* DestinationObject, FProperty* DestinationProperty);

	/**
	 * Copy the value of a property from source object to a destination object.
	 *
	 * @param	SourceObject			The object to copy the property value from.
	 * @param	SourcePropertyName		The name of the property on the SourceObject to copy the value from.
	 * @param	DestinationObject		The object to copy the property value to.
	 * @param	DestinationPropertyName	The name of the property on the DestinationObject to copy the value to.
	 *
	 * @return true if the value was successfully migrated.
	 */
	template <typename SourceType, typename DestinationType>
	bool MigratePropertyValue(SourceType* SourceObject, FName SourcePropertyName, DestinationType* DestinationObject, FName DestinationPropertyName)
	{
		FProperty* SourceProperty = FindFieldChecked<FProperty>(SourceType::StaticClass(), SourcePropertyName);
		FProperty* DestinationProperty = FindFieldChecked<FProperty>(DestinationType::StaticClass(), DestinationPropertyName);

		return FObjectEditorUtils::MigratePropertyValue(SourceObject, SourceProperty, DestinationObject, DestinationProperty);
	}

	/**
	 * Set the value on an UObject using reflection.
	 * @param	Object			The object to copy the value into.
	 * @param	PropertyName	The name of the property to set.
	 * @param	Value			The value to assign to the property.
	 *
	 * @return true if the value was set correctly
	 */
	template <typename ObjectType, typename ValueType>
	bool SetPropertyValue(ObjectType* Object, FName PropertyName, ValueType Value)
	{
		// Get the property addresses for the source and destination objects.
		FProperty* Property = FindFieldChecked<FProperty>(ObjectType::StaticClass(), PropertyName);

		// Get the property addresses for the object
		ValueType* SourceAddr = Property->ContainerPtrToValuePtr<ValueType>(Object);

		if ( SourceAddr == NULL )
		{
			return false;
		}

		if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			FEditPropertyChain PropertyChain;
			PropertyChain.AddHead(Property);
			Object->PreEditChange(PropertyChain);
		}

		// Set the value on the destination object.
		*SourceAddr = Value;

		if ( !Object->HasAnyFlags(RF_ClassDefaultObject) )
		{
			FPropertyChangedEvent PropertyEvent(Property);
			Object->PostEditChangeProperty(PropertyEvent);
		}

		return true;
	}

	/** Helper function to convert the input for GetActions to a list that can be used for delegates */
	template <typename T>
	static TArray<TWeakObjectPtr<T>> GetTypedWeakObjectPtrs(const TArray<UObject*>& InObjects)
	{
		check(InObjects.Num() > 0);

		TArray<TWeakObjectPtr<T>> TypedObjects;
		for(auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			TypedObjects.Add(CastChecked<T>(*ObjIt));
		}

		return TypedObjects;
	}
};

#endif // WITH_EDITOR
