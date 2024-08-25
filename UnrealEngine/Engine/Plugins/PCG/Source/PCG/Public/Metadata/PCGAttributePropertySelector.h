// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PCGPoint.h"

#include "Kismet/BlueprintFunctionLibrary.h"

#include "PCGAttributePropertySelector.generated.h"

UENUM()
enum class EPCGAttributePropertySelection
{
	Attribute,
	PointProperty,
	ExtraProperty
};

UENUM()
enum class EPCGExtraProperties : uint8
{
	Index
};

class FArchiveCrc32;
struct FPCGCustomVersion;
class UPCGData;

/**
* Blueprint class to allow to select an attribute or a property.
* It will handle the logic and can only be modified using the blueprint library defined below.
* Also has a custom detail view in the PCGEditor plugin.
* 
* Note: This class should not be used as is, but need to be referenced by either an "InputSelector" or an "OutputSelector" (defined below).
* The reason for that is to provide 2 different default values for input and output. Input will have the "@Last" default value (meaning last attribute written to)
* and the Output will have "@Source" default value (meaning, same thing as input).
*/
USTRUCT(BlueprintType, meta = (Hidden))
struct PCG_API FPCGAttributePropertySelector
{
	GENERATED_BODY()

public:
	virtual ~FPCGAttributePropertySelector() = default;

	bool operator==(const FPCGAttributePropertySelector& Other) const;
	bool IsSame(const FPCGAttributePropertySelector& Other, bool bIncludeExtraNames = true) const;

	// Setters, retrurn true if something changed.
	bool SetPointProperty(EPCGPointProperties InPointProperty, bool bResetExtraNames = true);
	bool SetExtraProperty(EPCGExtraProperties InExtraProperty, bool bResetExtraNames = true);
	bool SetAttributeName(FName InAttributeName, bool bResetExtraNames = true);

	// Getters
	EPCGAttributePropertySelection GetSelection() const { return Selection; }
	const TArray<FString>& GetExtraNames() const { return ExtraNames; }
	TArray<FString>& GetExtraNamesMutable() { return ExtraNames; }
	FName GetAttributeName() const { return AttributeName; }
	EPCGPointProperties GetPointProperty() const { return PointProperty; }
	EPCGExtraProperties GetExtraProperty() const { return ExtraProperty; }

	// Convenient function to know if it is a basic attribute (attribute and no extra names)
	bool IsBasicAttribute() const;

	// Return the name of the selector.
	FName GetName() const;

	// Returns the text to display in the widget.
	FText GetDisplayText() const;

	// Return true if the underlying name is valid.
	bool IsValid() const;

	// Update the selector with an incoming string.
	bool Update(const FString& NewValue);

	template <typename T>
	static T CreateFromOtherSelector(const FPCGAttributePropertySelector& InOther)
	{
		static_assert(std::is_base_of_v<FPCGAttributePropertySelector, T>, "The type must be of base class 'FPCGAttributePropertySelector'");
		T OutSelector;
		OutSelector.ImportFromOtherSelector(InOther);
		return OutSelector;
	}

	// Convenience templated static constructors
	template <typename T = FPCGAttributePropertySelector>
	static T CreateAttributeSelector(const FName AttributeName)
	{
		static_assert(std::is_base_of_v<FPCGAttributePropertySelector, T>, "The type must be of base class 'FPCGAttributePropertySelector'");
		T Selector;
		Selector.SetAttributeName(AttributeName);
		return Selector;
	}

	template <typename T = FPCGAttributePropertySelector>
	static T CreatePointPropertySelector(EPCGPointProperties PointProperty)
	{
		static_assert(std::is_base_of_v<FPCGAttributePropertySelector, T>, "The type must be of base class 'FPCGAttributePropertySelector'");
		T Selector;
		Selector.SetPointProperty(PointProperty);
		return Selector;
	}

	template <typename T = FPCGAttributePropertySelector>
	static T CreateExtraPropertySelector(EPCGExtraProperties ExtraProperty)
	{
		static_assert(std::is_base_of_v<FPCGAttributePropertySelector, T>, "The type must be of base class 'FPCGAttributePropertySelector'");
		T Selector;
		Selector.SetExtraProperty(ExtraProperty);
		return Selector;
	}

	template <typename T = FPCGAttributePropertySelector>
	static T CreateSelectorFromString(const FString& String)
	{
		static_assert(std::is_base_of_v<FPCGAttributePropertySelector, T>, "The type must be of base class 'FPCGAttributePropertySelector'");
		T Selector;
		Selector.Update(String);
		return Selector;
	}

	void ImportFromOtherSelector(const FPCGAttributePropertySelector& InOther);

	virtual void AddToCrc(FArchiveCrc32& Ar) const;

	friend uint32 GetTypeHash(const FPCGAttributePropertySelector& Selector);

protected:
	UPROPERTY()
	EPCGAttributePropertySelection Selection = EPCGAttributePropertySelection::Attribute;

	UPROPERTY()
	FName AttributeName = NAME_None;

	UPROPERTY()
	EPCGPointProperties PointProperty = EPCGPointProperties::Position;

	UPROPERTY()
	EPCGExtraProperties ExtraProperty = EPCGExtraProperties::Index;

	UPROPERTY()
	TArray<FString> ExtraNames;
};

/** Struct that will default on @Last (or @LastCreated for previously created selectors). */
USTRUCT(BlueprintType)
struct PCG_API FPCGAttributePropertyInputSelector : public FPCGAttributePropertySelector
{
	GENERATED_BODY()

	FPCGAttributePropertyInputSelector();

	/** Get a copy of the selector, with @Last replaced by the right selector. */
	FPCGAttributePropertyInputSelector CopyAndFixLast(const UPCGData* InData) const;

	/** To support previously saved nodes, that used FPCGAttributePropertySelector, we need to define this function to de-serialize the new class using the old. And add a trait (see below). */
	bool SerializeFromMismatchedTag(const struct FPropertyTag& Tag, FStructuredArchive::FSlot Slot);

	/** For older nodes, before the split between Input and Output, force any last attribute to be last created to preserve the old behavior. Will be called by the PCGSetting deprecation function. Not meant to be used otherwise. */
	void ApplyDeprecation(int32 InPCGCustomVersion);
};

/** Struct that will default on @Source. */
USTRUCT(BlueprintType)
struct PCG_API FPCGAttributePropertyOutputSelector : public FPCGAttributePropertySelector
{
	GENERATED_BODY()

	FPCGAttributePropertyOutputSelector();

	/** Get a copy of the selector, with @Source replaced by the right selector. Can add extra data for specific deprecation cases. */
	FPCGAttributePropertyOutputSelector CopyAndFixSource(const FPCGAttributePropertyInputSelector* InSourceSelector, const UPCGData* InOptionalData = nullptr) const;

	/** To support previously saved nodes, that used FPCGAttributePropertySelector, we need to define this function to de-serialize the new class using the old. And add a trait (see below). */
	bool SerializeFromMismatchedTag(const struct FPropertyTag& Tag, FStructuredArchive::FSlot Slot);
};

// Version where it doesn't make sense to have @Source, alias for FPCGAttributePropertySelector
USTRUCT(BlueprintType)
struct PCG_API FPCGAttributePropertyOutputNoSourceSelector : public FPCGAttributePropertySelector
{
	GENERATED_BODY()
};

// Extra trait to specify to the deserializer that those two classes can be deserialized using the old class.
template<>
struct TStructOpsTypeTraits<FPCGAttributePropertyInputSelector> : public TStructOpsTypeTraitsBase2<FPCGAttributePropertyInputSelector>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

template<>
struct TStructOpsTypeTraits<FPCGAttributePropertyOutputSelector> : public TStructOpsTypeTraitsBase2<FPCGAttributePropertyOutputSelector>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

/**
* Helper class to allow the BP to call the custom setters and getters on FPCGAttributePropertySelector.
*/
UCLASS()
class UPCGAttributePropertySelectorBlueprintHelpers : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static bool SetPointProperty(UPARAM(ref) FPCGAttributePropertySelector& Selector, EPCGPointProperties InPointProperty);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static bool SetAttributeName(UPARAM(ref) FPCGAttributePropertySelector& Selector, FName InAttributeName);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static bool SetExtraProperty(UPARAM(ref) FPCGAttributePropertySelector& Selector, EPCGExtraProperties InExtraProperty);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static EPCGAttributePropertySelection GetSelection(UPARAM(const, ref) const FPCGAttributePropertySelector& Selector);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static EPCGPointProperties GetPointProperty(UPARAM(const, ref) const FPCGAttributePropertySelector& Selector);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static FName GetAttributeName(UPARAM(const, ref) const FPCGAttributePropertySelector& Selector);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static EPCGExtraProperties GetExtraProperty(UPARAM(const, ref) const FPCGAttributePropertySelector& Selector);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static const TArray<FString>& GetExtraNames(UPARAM(const, ref) const FPCGAttributePropertySelector& Selector);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static FName GetName(UPARAM(const, ref) const FPCGAttributePropertySelector& Selector);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static FPCGAttributePropertyInputSelector CopyAndFixLast(UPARAM(const, ref) const FPCGAttributePropertyInputSelector& Selector, const UPCGData* InData);

	UFUNCTION(BlueprintCallable, Category = "PCG|PCGAttributePropertySelector", meta = (ScriptMethod))
	static FPCGAttributePropertyOutputSelector CopyAndFixSource(UPARAM(const, ref) const FPCGAttributePropertyOutputSelector& OutputSelector, const FPCGAttributePropertyInputSelector& InputSelector);
};