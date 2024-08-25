// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MovieGraphCommon.h"
#include "PropertyBag.h"
#include "Templates/ValueOrError.h"

#include "MovieGraphValueContainer.generated.h"

namespace UE::MovieGraph::Private
{
	template<typename ReturnType>
	bool GetOptionalValue(TValueOrError<ReturnType, EPropertyBagResult>& PropertyBagValue, ReturnType& OutValue)
	{
		// Convert the property bag-provided TValueOrError, which contains an EPropertyBagResult, to an output value and
		// a bool (signifying if there was an error or not). EPropertyBagResult shouldn't be exposed on the MRQ API.
		if (PropertyBagValue.HasValue())
		{
			OutValue = PropertyBagValue.StealValue();
			return true;
		}

		return false;
	}
}

/**
 * Holds a generic value, with an API for getting/setting the value, as well as getting/setting its type
 * and container (eg, array).
 */
UCLASS()
class MOVIERENDERPIPELINECORE_API UMovieGraphValueContainer : public UObject
{
	GENERATED_BODY()

public:
	UMovieGraphValueContainer();

	/** Gets the bool value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueBool(bool& bOutValue) const;

	/** Gets the byte value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueByte(uint8& OutValue) const;

	/** Gets the int32 value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueInt32(int32& OutValue) const;

	/** Gets the int64 value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueInt64(int64& OutValue) const;

	/** Gets the float value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueFloat(float& OutValue) const;

	/** Gets the double value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueDouble(double& OutValue) const;

	/** Gets the FName value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueName(FName& OutValue) const;

	/** Gets the FString value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueString(FString& OutValue) const;

	/** Gets the FText value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueText(FText& OutValue) const;

	/** Gets the enum value (for a specific enum) of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueEnum(uint8& OutValue, const UEnum* RequestedEnum = nullptr) const;

	/** Gets the struct value (for a specific struct) of the held data. Returns true on success, else false. */
	bool GetValueStruct(FStructView& OutValue, const UScriptStruct* RequestedStruct = nullptr) const;

	/** Gets the object value (for a specific class) of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueObject(UObject* OutValue, const UClass* RequestedClass = nullptr) const;

	/** Gets the UClass value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool GetValueClass(UClass*& OutValue) const;

	/** Gets the serialized string value of the held data. */
	UFUNCTION(BlueprintCallable, Category="Config")
	FString GetValueSerializedString();

	/** Gets the enum value of the held data. Returns true on success, else false. */
	template <typename T>
	bool GetValueEnum(T& OutValue) const
	{
		TValueOrError<T, EPropertyBagResult> Result = Value.GetValueEnum<T>(PropertyBagDefaultPropertyName);
		return UE::MovieGraph::Private::GetOptionalValue<T>(Result, OutValue);
	}

	/** Gets the struct value of the held data. Returns true on success, else false. */
	template <typename T>
	bool GetValueStruct(T* OutValue) const
	{
		TValueOrError<T*, EPropertyBagResult> Result = Value.GetValueStruct<T*>(PropertyBagDefaultPropertyName);
		return UE::MovieGraph::Private::GetOptionalValue<T*>(Result, OutValue);
	}

	/** Gets the object value of the held data. Returns true on success, else false. */
	template <typename T>
	bool GetValueObject(T* OutValue) const
	{
		TValueOrError<T*, EPropertyBagResult> Result = Value.GetValueObject<T*>(PropertyBagDefaultPropertyName);
		return UE::MovieGraph::Private::GetOptionalValue<T*>(Result, OutValue);
	}

	/** Sets the bool value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueBool(const bool bInValue);

	/** Sets the byte value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueByte(const uint8 InValue);

	/** Sets the int32 value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueInt32(const int32 InValue);

	/** Sets the int64 value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueInt64(const int64 InValue);

	/** Sets the float value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueFloat(const float InValue);

	/** Sets the double value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueDouble(const double InValue);

	/** Sets the FName value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueName(const FName InValue);

	/** Sets the FString value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueString(const FString& InValue);

	/** Sets the FText value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueText(const FText& InValue);

	/** Sets the enum value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueEnum(const uint8 InValue, const UEnum* Enum);

	/** Sets the struct value of the held data. Returns true on success, else false. */
	bool SetValueStruct(FConstStructView InValue);

	/** Sets the object value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueObject(UObject* InValue);

	/** Sets the class value of the held data. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueClass(UClass* InValue);

	/** Sets the serialized value of the held data. The string should be the serialized representation of the value. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Config")
	bool SetValueSerializedString(const FString& NewValue);

	/** Sets the enum value of the held data. Returns true on success, else false. */
	template <typename T>
	bool SetValueEnum(const T InValue)
	{
		return Value.SetValueEnum<T>(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
	}

	/** Sets the struct value of the held data. Returns true on success, else false. */
	template <typename T>
	bool SetValueStruct(const T& InValue)
	{
		return Value.SetValueStruct<T>(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
	}

	/** Sets the object value of the held data. Returns true on success, else false. */
	template <typename T>
	bool SetValueObject(T* InValue)
	{
		return Value.SetValueObject<T>(PropertyBagDefaultPropertyName, InValue) == EPropertyBagResult::Success;
	}

	/** Gets the type of the stored data. */
	UFUNCTION(BlueprintCallable, Category="Config")
	EMovieGraphValueType GetValueType() const;

	/** Sets the type of the stored data. Enums, structs, and classes must specify a value type object. */
	UFUNCTION(BlueprintCallable, Category="Config")
	void SetValueType(EMovieGraphValueType ValueType, UObject* InValueTypeObject = nullptr);

	/** Gets the object that defines the enum, struct, or class. */
	UFUNCTION(BlueprintCallable, Category="Config")
	const UObject* GetValueTypeObject() const;

	/** Sets the object that defines the enum, struct, or class. */
	UFUNCTION(BlueprintCallable, Category="Config")
	void SetValueTypeObject(const UObject* ValueTypeObject);

	/** Gets the container type of the stored value. */
	UFUNCTION(BlueprintCallable, Category="Config")
	EMovieGraphContainerType GetValueContainerType() const;

	/** Sets the container type of the stored value. */
	UFUNCTION(BlueprintCallable, Category="Config")
	void SetValueContainerType(EMovieGraphContainerType ContainerType);

	/**
	 * Gets a reference to the array backing the value, if any. GetValueContainerType() will return
	 * EMovieGraphContainerType::Array if the value is holding an array.
	 */
	TValueOrError<FPropertyBagArrayRef, EPropertyBagResult> GetArrayRef();

	/**
	 * Sets the name of the property that the value container holds. This is mostly unneeded unless the display name
	 * of the property is important (eg, in the details panel).
	 */
	void SetPropertyName(const FName& InName);

	/** Gets the name of the property that the value container holds. */
	FName GetPropertyName() const;

private:
	/**
	 * Sets the configuration of this container from a property desc. This is less safe than the strongly-typed methods,
	 * and is reserved only for UMovieJobVariableAssignmentContainer to call. The serialized representation of the value
	 * needs to be supplied via InString.
	 */
	friend class UMovieJobVariableAssignmentContainer;
	void SetFromDesc(const FPropertyBagPropertyDesc* InDesc, const FString& InString);

private:
	/** The default name of the single property stored in the property bag. */
	static const FName PropertyBagDefaultPropertyName;

	/** The name of the single property stored in the property bag. */
	UPROPERTY()
	FName PropertyName;
	
	// Note: The property bag only stores one property, since the object only needs to store one value. This is an odd
	// use of a property bag, but the property bag solves a number of issues that would be very difficult to solve
	// otherwise. 1) Presentation of a property that can have its type changed within the details pane, 2) data storage
	// of a property which can have its type changed, 3) the ability to set the value of the property from both Python
	// and C++, and 4) the ability to change the property at runtime.
	/** The value held by this object. */
	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties, FixedLayout), Category = "Value")
	FInstancedPropertyBag Value;
};