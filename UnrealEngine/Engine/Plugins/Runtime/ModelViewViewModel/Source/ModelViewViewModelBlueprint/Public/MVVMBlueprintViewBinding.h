// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMPropertyPath.h"
#include "Types/MVVMBindingMode.h"

#include "MVVMBlueprintViewBinding.generated.h"

class UWidgetBlueprint;
class UMVVMBlueprintView;

/**
*
*/
USTRUCT(BlueprintType)
struct MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintViewConversionPath
{
	GENERATED_BODY()

	/** The Conversion function when converting the value from the destination to the source. */
	UPROPERTY(EditAnywhere, Category = "MVVM", AdvancedDisplay)
	FMemberReference DestinationToSourceFunction;
	
	UPROPERTY()
	FName DestinationToSourceWrapper;

	/** The Conversion function when converting the value from the source to the destination. */
	UPROPERTY(EditAnywhere, Category = "MVVM", AdvancedDisplay)
	FMemberReference SourceToDestinationFunction;

	UPROPERTY()
	FName SourceToDestinationWrapper;
};

/**
*
*/
USTRUCT(BlueprintType)
struct MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintViewBinding
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "MVVM")
	FMVVMBlueprintPropertyPath ViewModelPath;

	UPROPERTY(EditAnywhere, Category = "MVVM")
	FMVVMBlueprintPropertyPath WidgetPath;

	/** */
	UPROPERTY(EditAnywhere, Category = "MVVM")
	EMVVMBindingMode BindingType = EMVVMBindingMode::OneWayToDestination;

	UPROPERTY(EditAnywhere, Category = "MVVM")
	EMVVMViewBindingUpdateMode UpdateMode = EMVVMViewBindingUpdateMode::Immediate;

	/** */
	UPROPERTY(EditAnywhere, Category = "MVVM")
	FMVVMBlueprintViewConversionPath Conversion;

	/** */
	UPROPERTY(VisibleAnywhere, Category = "MVVM", Transient)
	TArray<FText> Errors;

	/** The unique ID of this binding. */
	FGuid BindingId;

	/** Whether the binding is enabled or disabled by default. The instance may enable the binding at runtime. */
	UPROPERTY(EditAnywhere, Category = "MVVM")
	bool bEnabled = true;

	/** The binding is visible in the editor, but is not compiled and cannot be used at runtime. */
	UPROPERTY(EditAnywhere, Category = "MVVM")
	bool bCompile = true;

	/**
	 * Get an internal name. For use in the UI, use GetDisplayNameString()
	 */
	FName GetFName() const;

	/** 
	 * Get a string that identifies this binding. 
	 * This is of the form: Widget.Property <- ViewModel.Property
	 */
	FString GetDisplayNameString(const UWidgetBlueprint* WidgetBlueprint) const;
};
