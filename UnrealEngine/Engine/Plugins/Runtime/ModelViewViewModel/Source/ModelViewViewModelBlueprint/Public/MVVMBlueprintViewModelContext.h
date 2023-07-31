// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Templates/SubclassOf.h"
#include "MVVMViewModelBase.h"

#include "MVVMBlueprintViewModelContext.generated.h"

/**
 *
 */
UENUM()
enum class EMVVMBlueprintViewModelContextCreationType : uint8
{
	Manual,	// The viewmodel will be assigned later.
	CreateInstance, // A new instance of the viewmodel will be created when the widget is created.
	GlobalViewModelCollection, // The viewmodel exists and is added to the MVVMSubsystem. It will be fetched there.
	PropertyPath, // The viewmodel will be fetched by evaluating a function or a property path.
};

/**
 *
 */
USTRUCT(BlueprintType)
struct MODELVIEWVIEWMODELBLUEPRINT_API FMVVMBlueprintViewModelContext
{
	GENERATED_BODY()

public:
	FMVVMBlueprintViewModelContext() = default;
	FMVVMBlueprintViewModelContext(const UClass* InClass, FName InViewModelName);

	FGuid GetViewModelId() const
	{
		return ViewModelContextId;
	}

	FName GetViewModelName() const
	{
		return ViewModelName;
	}

	FText GetDisplayName() const;

	UClass* GetViewModelClass() const
	{
		return NotifyFieldValueClass;
	}

	void PostSerialize(const FArchive& Ar)
	{
		if (Ar.IsLoading())
		{
			if (ViewModelName.IsNone())
			{
				ViewModelName = *OverrideDisplayName_DEPRECATED.ToString();
			}
			if (ViewModelName.IsNone() )
			{
				ViewModelName = *ViewModelContextId.ToString();
			}
			if (ViewModelClass_DEPRECATED.Get())
			{
				NotifyFieldValueClass = ViewModelClass_DEPRECATED.Get();
			}
		}
	}

	bool IsValid() const
	{
		return NotifyFieldValueClass != nullptr;
	}

private:
	/** When the view is spawn, create an instance of the viewmodel. */
	UPROPERTY(VisibleAnywhere, AdvancedDisplay, Category = "MVVM")
	FGuid ViewModelContextId;

	UPROPERTY(VisibleAnywhere, Category = "MVVM", NoClear, meta = (AllowedClasses = "/Script/UMG.NotifyFieldValueChanged", DisallowedClasses = "/Script/UMG.Widget"))
	TObjectPtr<UClass> NotifyFieldValueClass = nullptr;

	UPROPERTY()
	TSubclassOf<UMVVMViewModelBase> ViewModelClass_DEPRECATED;

	UPROPERTY()
	FText OverrideDisplayName_DEPRECATED;

public:
	/** Property name that will be generated. */
	UPROPERTY(EditAnywhere, Category = "MVVM")
	FName ViewModelName;

	/** When the view is spawn, create an instance of the viewmodel. */
	UPROPERTY(EditAnywhere, Category = "MVVM")
	EMVVMBlueprintViewModelContextCreationType CreationType = EMVVMBlueprintViewModelContextCreationType::CreateInstance;

	/** Identifier of an already registered viewmodel. */
	UPROPERTY(EditAnywhere, Category = "MVVM", AdvancedDisplay, meta = (EditCondition = "CreationType == EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection"))
	FName GlobalViewModelIdentifier;

	/** The Path to get the viewmodel instance. */
	UPROPERTY(EditAnywhere, Category = "MVVM", AdvancedDisplay, meta = (EditCondition = "CreationType == EMVVMBlueprintViewModelContextCreationType::PropertyPath"))
	FString ViewModelPropertyPath;

	/**
	 * Generate a setter function for this viewmodel.
	 * @note Always true when the Creation Type is Manual.
	 */
	UPROPERTY(EditAnywhere, Category = "MVVM", AdvancedDisplay, meta = (EditCondition = "CreationType != EMVVMBlueprintViewModelContextCreationType::Manual"))
	bool bCreateSetterFunction = false;

	/**
	 * Optional. Will not warn if the instance is not set or found.
	 * @note Always true when the Creation Type is Manual.
	 */
	UPROPERTY(EditAnywhere, Category = "MVVM", AdvancedDisplay, meta = (EditCondition = "CreationType == EMVVMBlueprintViewModelContextCreationType::GlobalViewModelCollection || CreationType == EMVVMBlueprintViewModelContextCreationType::PropertyPath"))
	bool bOptional = false;
};

template<>
struct TStructOpsTypeTraits<FMVVMBlueprintViewModelContext> : public TStructOpsTypeTraitsBase2<FMVVMBlueprintViewModelContext>
{
	enum
	{
		WithPostSerialize = true,
	};
};