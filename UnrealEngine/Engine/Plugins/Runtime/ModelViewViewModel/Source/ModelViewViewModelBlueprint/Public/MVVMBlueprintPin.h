// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMPropertyPath.h"

#include "MVVMBlueprintPin.generated.h"

class UWidgetBlueprint;
class UEdGraphNode;
class UEdGraphPin;

/**
*
*/
UENUM()
enum class EMVVMBlueprintPinStatus : uint8
{
	Valid,
	Orphaned,
};

/**
 * Pin name type to help with compare operation and moving it around.
 */
USTRUCT()
struct MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintPinId
{
	GENERATED_BODY()

	FMVVMBlueprintPinId() = default;
	explicit FMVVMBlueprintPinId(const TArrayView<const FName> Names);
	explicit FMVVMBlueprintPinId(TArray<FName>&& Names);

	bool IsValid() const;

	const TArrayView<const FName> GetNames() const
	{
		return PinNames;
	}

	/** return true if the Pin is part of the Other pin. It can be a grand child. */
	bool IsChildOf(const FMVVMBlueprintPinId& Other) const;

	/** return true if the Pin is the directly child of the Other pin. It can be a child but not a grand child. */
	bool IsDirectChildOf(const FMVVMBlueprintPinId& Other) const;

	bool operator==(const FMVVMBlueprintPinId& Other) const;
	bool operator==(const TArrayView<const FName> Other) const;

	FString ToString() const;

private:
	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	TArray<FName> PinNames;
};

/**
 *
 */
USTRUCT()
struct MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintPin
{
	GENERATED_BODY()

private:	
	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	FMVVMBlueprintPinId Id;

	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	FMVVMBlueprintPropertyPath Path;

	/** Default value for this pin (used if the pin has no connections), stored as a string */
	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	FString DefaultString;

	/** If the default value for this pin should be an FText, it is stored here. */
	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	FText DefaultText;

	/** If the default value for this pin should be an object, we store a pointer to it */
	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	TObjectPtr<class UObject> DefaultObject;

	/** The pin is split. */
	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	bool bSplit = false;

	/** The pin could not be set. */
	UPROPERTY(VisibleAnywhere, Category = "MVVM")
	mutable EMVVMBlueprintPinStatus Status = EMVVMBlueprintPinStatus::Valid;

	UPROPERTY()
	FName PinName_DEPRECATED;
	
	UPROPERTY()
	FGuid PinId_DEPRECATED;

public:
	FMVVMBlueprintPin() = default;
	UE_DEPRECATED(5.4, "FMVVMBlueprintPin with a single name is deprecated. Use the TArrayView constructor instead")
	FMVVMBlueprintPin(FName PinName);
	explicit FMVVMBlueprintPin(FMVVMBlueprintPinId PinId);
	explicit FMVVMBlueprintPin(const TArrayView<const FName> PinName);

	UE_DEPRECATED(5.4, "GetName is deprecated. Use GetNames instead")
	FName GetName() const
	{
		return Id.GetNames().Num() > 0 ? Id.GetNames().Last() : FName();
	}

	const FMVVMBlueprintPinId& GetId() const
	{
		return Id;
	}

	bool IsValid() const
	{
		return Id.IsValid();
	}

	/** The pin is split into its different components. */
	bool IsSplit() const
	{
		return bSplit;
	}

	/** The pin could not be assigned to the graph pin. */
	EMVVMBlueprintPinStatus GetStatus() const
	{
		return Status;
	}

	/** Are we using the path. */
	bool UsedPathAsValue() const
	{
		return !bSplit && Path.IsValid();
	}

	/** Get the path used by this pin. */
	const FMVVMBlueprintPropertyPath& GetPath() const
	{
		return Path;
	}

	FString GetValueAsString(const UClass* SelfContext) const;

	void SetDefaultValue(UObject* Value);
	void SetDefaultValue(const FText& Value);
	void SetDefaultValue(const FString& Value);
	void SetPath(const FMVVMBlueprintPropertyPath& Value);

public:
	void PostSerialize(const FArchive& Ar);

public:
	static bool IsInputPin(const UEdGraphPin* Pin);
	static TArray<FMVVMBlueprintPin> CopyAndReturnMissingPins(UBlueprint* Blueprint, UEdGraphNode* GraphNode, const TArray<FMVVMBlueprintPin>& Pins);
	static TArray<FMVVMBlueprintPin> CreateFromNode(UBlueprint* Blueprint, UEdGraphNode* GraphNode);
	static FMVVMBlueprintPin CreateFromPin(const UBlueprint* Blueprint, const UEdGraphPin* Pin);

	void CopyTo(const UBlueprint* WidgetBlueprint, UEdGraphNode* Node) const;
	UEdGraphPin* FindGraphPin(const UEdGraph* Graph) const;

	void Reset();
};

template<>
struct TStructOpsTypeTraits<FMVVMBlueprintPin> : public TStructOpsTypeTraitsBase2<FMVVMBlueprintPin>
{
	enum
	{
		WithPostSerialize = true,
	};
};
