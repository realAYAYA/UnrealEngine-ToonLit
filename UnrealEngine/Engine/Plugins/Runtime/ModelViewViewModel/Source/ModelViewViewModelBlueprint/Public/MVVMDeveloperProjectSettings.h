// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "MVVMDeveloperProjectSettings.generated.h"

class UK2Node;
enum class EMVVMBlueprintViewModelContextCreationType : uint8;
enum class EMVVMExecutionMode : uint8;

/**
 * 
 */
USTRUCT()
struct FMVVMDeveloperProjectWidgetSettings
{
	GENERATED_BODY()

	/** Properties or functions name that should not be use for binding (read or write). */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	TSet<FName> DisallowedFieldNames;
	
	/** Properties or functions name that are displayed in the advanced category. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	TSet<FName> AdvancedFieldNames;
};

UENUM()
enum class EFilterFlag : uint8
{
	None = 0,
	All = 1 << 0
};
ENUM_CLASS_FLAGS(EFilterFlag)

USTRUCT()
struct FMVVMViewBindingFilterSettings
{
	GENERATED_BODY()

	/** Filter out the properties and functions that are not valid in the context of the binding. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	EFilterFlag FilterFlags = EFilterFlag::None;
};

/**
 *
 */
UENUM()
enum class EMVVMDeveloperConversionFunctionFilterType : uint8
{
	BlueprintActionRegistry,
	AllowedList,
};


/**
 * Implements the settings for the MVVM Editor
 */
UCLASS(config=ModelViewViewModel, defaultconfig)
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMDeveloperProjectSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UMVVMDeveloperProjectSettings();

	virtual FName GetCategoryName() const override;
	virtual FText GetSectionText() const override;


	bool PropertyHasFiltering(const UStruct* ObjectStruct, const FProperty* Property) const;
	bool IsPropertyAllowed(const UBlueprint* Context, const UStruct* ObjectStruct, const FProperty* Property) const;
	bool IsFunctionAllowed(const UBlueprint* Context, const UClass* ObjectClass, const UFunction* Function) const;
	bool IsConversionFunctionAllowed(const UBlueprint* Context, const UFunction* Function) const;
	bool IsConversionFunctionAllowed(const UBlueprint* Context, const TSubclassOf<UK2Node> Function) const;

	bool IsExecutionModeAllowed(EMVVMExecutionMode ExecutionMode) const
	{
		return AllowedExecutionMode.Contains(ExecutionMode);
	}

	bool IsContextCreationTypeAllowed(EMVVMBlueprintViewModelContextCreationType ContextCreationType) const
	{
		return AllowedContextCreationType.Contains(ContextCreationType);
	}

	EMVVMDeveloperConversionFunctionFilterType GetConversionFunctionFilter() const
	{
		return ConversionFunctionFilter;
	}

	TArray<const UClass*> GetAllowedConversionFunctionClasses() const;

private:
	/** Permission list for filtering which properties are visible in UI. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	TMap<FSoftClassPath, FMVVMDeveloperProjectWidgetSettings> FieldSelectorPermissions;

	/** Permission list for filtering which execution mode is allowed. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	TSet<EMVVMExecutionMode> AllowedExecutionMode;
	
	/** Permission list for filtering which context creation type is allowed. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	TSet<EMVVMBlueprintViewModelContextCreationType> AllowedContextCreationType;

public:
	/** Binding can be made from the DetailView Bind option. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bAllowBindingFromDetailView = true;

	/** When generating a source in the viewmodel editor, allow the compiler to generate a setter function. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bAllowGeneratedViewModelSetter = true;

	/** When generating a binding with a long source path, allow the compiler to generate a new viewmodel source. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bAllowLongSourcePath = true;
	
	/** For the binding list widget, allow the user to edit the binding in the detail view. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bShowDetailViewOptionInBindingPanel = true;
	
	/** For the binding list widget and the viewmodel panel, allow the user to edit the view settings in the detail view. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bShowViewSettings = true;

	/** For the binding list widget, allow the user to generate a copy of the binding/event graph. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bShowDeveloperGenerateGraphSettings = true;

	/** When a conversion function requires a wrapper graph, add and save the generated graph to the blueprint. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bAllowConversionFunctionGeneratedGraphInEditor = false;

	/** When binding to a multicast delegate property, allow to create an event. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bAllowBindingEvent = true;
	
	/** Allow to create an instanced viewmodel directly in the view editor. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bCanCreateViewModelInView = false;

	/** 
	 * When a viewmodel is set to Create Instance, allow modifying the viewmodel instance in the editor on all instances of the owning widget.
	 * The per-viewmodel setting "Expose Instance In Editor" overrides this.
	 */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	bool bExposeViewModelInstanceInEditor = false;

	/** Permission list for filtering which execution mode is allowed. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	EMVVMDeveloperConversionFunctionFilterType ConversionFunctionFilter = EMVVMDeveloperConversionFunctionFilterType::BlueprintActionRegistry;

	/** Individual class that are allowed to be uses as conversion functions. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel", meta = (EditCondition = "ConversionFunctionFilter == EMVVMDeveloperConversionFunctionFilterType::AllowedList"))
	TSet<FSoftClassPath> AllowedClassForConversionFunctions;

	/** Settings for filtering the list of available properties and functions on binding creation. */
	UPROPERTY(EditAnywhere, config, Category = "Viewmodel")
	FMVVMViewBindingFilterSettings FilterSettings;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Components/Widget.h"
#include "CoreMinimal.h"
#endif
