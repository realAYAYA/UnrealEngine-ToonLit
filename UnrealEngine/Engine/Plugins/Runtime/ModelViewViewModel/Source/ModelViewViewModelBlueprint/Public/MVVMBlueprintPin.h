// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMPropertyPath.h"

#include "MVVMBlueprintPin.generated.h"

class UWidgetBlueprint;
class UEdGraphPin;

/**
*
*/
USTRUCT()
struct MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintPin
{
	GENERATED_BODY()

private:
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FName PinName;

	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FMVVMBlueprintPropertyPath Path;

	/** Default value for this pin (used if the pin has no connections), stored as a string */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FString DefaultString;

	/** If the default value for this pin should be an FText, it is stored here. */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	FText DefaultText;

	/** If the default value for this pin should be an object, we store a pointer to it */
	UPROPERTY(VisibleAnywhere, Category = "Viewmodel")
	TObjectPtr<class UObject> DefaultObject;

public:
	FMVVMBlueprintPin() = default;

	FName GetName() const
	{
		return PinName;
	}

	/** Are we using the path. */
	bool UsedPathAsValue() const
	{
		return !Path.IsEmpty();
	}

	const FMVVMBlueprintPropertyPath& GetPath() const
	{
		return Path;
	}

	FString GetValueAsString(const UClass* SelfContext) const;

	void SetDefaultValue(UObject* Value);
	void SetDefaultValue(const FText& Value);
	void SetDefaultValue(const FString& Value);
	void SetPath(const FMVVMBlueprintPropertyPath& Value);

	static FMVVMBlueprintPin CreateFromPin(const UBlueprint* WidgetBlueprint, const UEdGraphPin* Pin);
	void CopyTo(const UBlueprint* WidgetBlueprint, UEdGraphPin* Pin) const;

private:
	void Reset();
};
