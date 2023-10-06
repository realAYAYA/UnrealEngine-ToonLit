// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Graph/MovieGraphCommon.h"
#include "Graph/MovieGraphValueContainer.h"	// For GetOptionalValue()
#include "PropertyBag.h"

#include "MovieJobVariableAssignmentContainer.generated.h"

class UMovieGraphConfig;
class UMovieGraphVariable;

/**
 * Holds a group of properties which override variable values on the job's associated graph (if any). Overrides are not
 * added manually. Instead, UpdateGraphVariableOverrides() should be called which will update, add, or remove overrides
 * as appropriate. After the update, overrides can have their values retrieved and set.
 */
UCLASS(BlueprintType)
class MOVIERENDERPIPELINECORE_API UMovieJobVariableAssignmentContainer : public UObject
{
	GENERATED_BODY()

public:
	UMovieJobVariableAssignmentContainer() = default;

	/**
	 * Sets the graph config associated with the variable assignments. Calls UpdateGraphVariableOverrides() to ensure
	 * that the overrides reflect the specified graph config.
	 */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	void SetGraphConfig(const TSoftObjectPtr<UMovieGraphConfig>& InGraphConfig);

	/** Gets the number of variable assignments present in this container. Assignments that are disabled are counted. */
	uint32 GetNumAssignments() const;

	/** Gets the bool value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueBool(const FName& PropertyName, bool& bOutValue) const;

	/** Gets the byte value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueByte(const FName& PropertyName, uint8& OutValue) const;

	/** Gets the int32 value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueInt32(const FName& PropertyName, int32& OutValue) const;

	/** Gets the int64 value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueInt64(const FName& PropertyName, int64& OutValue) const;

	/** Gets the float value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueFloat(const FName& PropertyName, float& OutValue) const;

	/** Gets the double value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueDouble(const FName& PropertyName, double& OutValue) const;

	/** Gets the FName value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueName(const FName& PropertyName, FName& OutValue) const;

	/** Gets the FString value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueString(const FName& PropertyName, FString& OutValue) const;

	/** Gets the FText value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueText(const FName& PropertyName, FText& OutValue) const;

	/** Gets the enum value (for a specific enum) of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueEnum(const FName& PropertyName, uint8& OutValue, const UEnum* RequestedEnum = nullptr) const;

	/** Gets the struct value (for a specific struct) of the specified property. Returns true on success, else false. */
	bool GetValueStruct(const FName& PropertyName, FStructView& OutValue, const UScriptStruct* RequestedStruct = nullptr) const;

	/** Gets the object value (for a specific class) of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueObject(const FName& PropertyName, UObject* OutValue, const UClass* RequestedClass = nullptr) const;

	/** Gets the UClass value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool GetValueClass(const FName& PropertyName, UClass* OutValue) const;

	/** Gets the serialized string value of the specified property. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	FString GetValueSerializedString(const FName& PropertyName);

	/** Gets the enum value of the specified property. Returns true on success, else false. */
	template <typename T>
	bool GetValueEnum(const FName& PropertyName, T& OutValue) const
	{
		TValueOrError<T, EPropertyBagResult> Result = Value.GetValueEnum<T>(PropertyName);
		return UE::MovieGraph::Private::GetOptionalValue<T>(Result, OutValue);
	}

	/** Gets the struct value of the specified property. Returns true on success, else false. */
	template <typename T>
	bool GetValueStruct(const FName& PropertyName, T* OutValue) const
	{
		TValueOrError<T*, EPropertyBagResult> Result = Value.GetValueStruct<T*>(PropertyName);
		return UE::MovieGraph::Private::GetOptionalValue<T*>(Result, OutValue);
	}

	/** Gets the object value of the specified property. Returns true on success, else false. */
	template <typename T>
	bool GetValueObject(const FName& PropertyName, T* OutValue) const
	{
		TValueOrError<T*, EPropertyBagResult> Result = Value.GetValueObject<T*>(PropertyName);
		return UE::MovieGraph::Private::GetOptionalValue<T*>(Result, OutValue);
	}

	/** Sets the bool value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueBool(const FName& PropertyName, const bool bInValue);

	/** Sets the byte value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueByte(const FName& PropertyName, const uint8 InValue);

	/** Sets the int32 value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueInt32(const FName& PropertyName, const int32 InValue);

	/** Sets the int64 value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueInt64(const FName& PropertyName, const int64 InValue);

	/** Sets the float value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueFloat(const FName& PropertyName, const float InValue);

	/** Sets the double value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueDouble(const FName& PropertyName, const double InValue);

	/** Sets the FName value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueName(const FName& PropertyName, const FName InValue);

	/** Sets the FString value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueString(const FName& PropertyName, const FString& InValue);

	/** Sets the FText value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueText(const FName& PropertyName, const FText& InValue);

	/** Sets the enum value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueEnum(const FName& PropertyName, const uint8 InValue, const UEnum* Enum);

	/** Sets the struct value of the specified property. Returns true on success, else false. */
	bool SetValueStruct(const FName& PropertyName, FConstStructView InValue);

	/** Sets the object value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueObject(const FName& PropertyName, UObject* InValue);

	/** Sets the class value of the specified property. Returns true on success, else false. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueClass(const FName& PropertyName, UClass* InValue);

	/** Sets the serialized value of this member. The string should be the serialized representation of the value. Returns true on success, else false.*/
	UFUNCTION(BlueprintCallable, Category="Experimental")
	bool SetValueSerializedString(const FName& PropertyName, const FString& NewValue);

	/** Sets the enum value of the specified property. Returns true on success, else false. */
	template <typename T>
	bool SetValueEnum(const FName& PropertyName, const T InValue)
	{
		return Value.SetValueEnum<T>(PropertyName, InValue) == EPropertyBagResult::Success;
	}

	/** Sets the struct value of the specified property. Returns true on success, else false. */
	template <typename T>
	bool SetValueStruct(const FName& PropertyName, const T& InValue)
	{
		return Value.SetValueStruct<T>(PropertyName, InValue) == EPropertyBagResult::Success;
	}

	/** Sets the object value of the specified property. Returns true on success, else false. */
	template <typename T>
	bool SetValueObject(const FName& PropertyName, T* InValue)
	{
		return Value.SetValueObject<T>(PropertyName, InValue) == EPropertyBagResult::Success;
	}

	/** Gets the type of the value stored in the specified property. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	EMovieGraphValueType GetValueType(const FName& PropertyName) const;

	/** Gets the object that defines the enum, struct, or class stored in the specified property. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	const UObject* GetValueTypeObject(const FName& PropertyName) const;

	/** Gets the container type of the stored value in the specified property. */
	UFUNCTION(BlueprintCallable, Category="Experimental")
	EMovieGraphContainerType GetValueContainerType(const FName& PropertyName) const;

	/**
	 * Updates an existing variable assignment for the provided graph variable to a new enable state, or adds a new
	 * assignment and updates its enable state. Returns true on success, else false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Experimental")
	bool SetVariableAssignmentEnableState(const UMovieGraphVariable* InGraphVariable, bool bIsEnabled);

	/**
	 * Gets the enable state of the variable assignment for the provided graph variable. The enable state is provided
	 * via bOutIsEnabled. Returns true if an enable state was set on the variable and bOutIsEnabled was changed, else false.
	 */
	UFUNCTION(BlueprintCallable, Category = "Experimental")
	bool GetVariableAssignmentEnableState(const UMovieGraphVariable* InGraphVariable, bool& bOutIsEnabled);

	/**
	 * Updates the stored variable overrides to reflect the graph preset. Existing overrides will be updated to match
	 * the graph variable name, value type, object type, and container type. Additionally, stale overrides that have no
	 * corresponding graph variable will be removed, and overrides will be created for graph variables which do not have
	 * existing overrides.
	 */
#if WITH_EDITOR
	UFUNCTION(BlueprintCallable, Category = "Experimental")
	void UpdateGraphVariableOverrides();
#endif

protected:
	/**
	 * Add a variable assignment for the graph. Returns true if the variable was added or it already exists,
	 * else false.
	 */
	bool AddVariableAssignment(const UMovieGraphVariable* InGraphVariable);
	
	/**
	 * Finds a variable assignment for the provided variable, or adds one if one does not already exist (and
	 * bAddIfNotExists is set to true). Returns nullptr if the operation failed. 
	 */
	bool FindOrGenerateVariableOverride(const UMovieGraphVariable* InGraphVariable, FPropertyBagPropertyDesc* OutPropDesc = nullptr,
		FPropertyBagPropertyDesc* OutEditConditionPropDesc = nullptr, bool bGenerateIfNotExists = true);

	/** Generates a variable override for the provided variable, as well as the associated EditCondition for it. */
	bool GenerateVariableOverride(const UMovieGraphVariable* InGraphVariable, FPropertyBagPropertyDesc* OutPropDesc = nullptr,
		FPropertyBagPropertyDesc* OutEditConditionPropDesc = nullptr);

private:
	/** The properties managed by this object. */
	UPROPERTY(EditAnywhere, meta=(ShowOnlyInnerProperties, FixedLayout), Category = "Value")
	FInstancedPropertyBag Value;

	/** The graph preset associated with the variable overrides. */
	UPROPERTY()
	TSoftObjectPtr<UMovieGraphConfig> GraphPreset;
};