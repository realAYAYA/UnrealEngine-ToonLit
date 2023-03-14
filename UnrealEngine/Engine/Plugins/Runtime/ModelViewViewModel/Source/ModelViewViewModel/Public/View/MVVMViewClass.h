// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "Blueprint/UserWidget.h"
#include "Bindings/MVVMCompiledBindingLibrary.h"
#include "Extensions/WidgetBlueprintGeneratedClassExtension.h"
#include "FieldNotification/FieldId.h"
#include "Types/MVVMBindingMode.h"
#include "Types/MVVMBindingName.h"
#include "Types/MVVMViewModelContext.h"
#include "View/MVVMView.h"

#include "MVVMViewClass.generated.h"


class UMVVMUserWidgetBinding;
class UMVVMView;
class UMVVMViewClass;
class UMVVMViewModelBlueprintExtension;
class UUserWidget;

namespace UE::MVVM::Private
{
	struct FMVVMViewBlueprintCompiler;
}

/**
 * Shared data to find or create a ViewModel at runtime.
 */
USTRUCT()
struct FMVVMViewClass_SourceCreator
{
	GENERATED_BODY()

	friend UE::MVVM::Private::FMVVMViewBlueprintCompiler;

public:
	MODELVIEWVIEWMODEL_API static FMVVMViewClass_SourceCreator MakeManual(FName Name, UClass* NotifyFieldValueChangedClass);
	MODELVIEWVIEWMODEL_API static FMVVMViewClass_SourceCreator MakeInstance(FName Name, UClass* NotifyFieldValueChangedClass);
	MODELVIEWVIEWMODEL_API static FMVVMViewClass_SourceCreator MakeFieldPath(FName Name, UClass* NotifyFieldValueChangedClass, FMVVMVCompiledFieldPath FieldPath, bool bOptional);
	MODELVIEWVIEWMODEL_API static FMVVMViewClass_SourceCreator MakeGlobalContext(FName Name, FMVVMViewModelContext Context, bool bOptional);

	UObject* CreateInstance(const UMVVMViewClass* ViewClass, UMVVMView* View, UUserWidget* UserWidget) const;

	UClass* GetSourceClass() const
	{
		return ExpectedSourceType.Get();
	}

	FName GetSourcePropertyName() const
	{
		return PropertyName;
	}

private:
	/** Class type to create a source at runtime. */
	UPROPERTY()
	TSubclassOf<UObject> ExpectedSourceType;

	/** Info to find the ViewModel instance at runtime. */
	UPROPERTY()
	FMVVMViewModelContext GlobalViewModelInstance;

	/**
	 * A resolvable path to retrieve the source instance at runtime.
	 * It can be a path "Property = Object.Function.Object".
	 * It can be a UFunction's name of a FProperty's name.
	 */
	UPROPERTY()
	FMVVMVCompiledFieldPath FieldPath;

	UPROPERTY()
	FName PropertyName;
	
	UPROPERTY()
	bool bCreateInstance = false;

	UPROPERTY()
	bool bOptional = false;
};


/**
 * A compiled and shared binding for ViewModel<->View
 */
USTRUCT()
struct MODELVIEWVIEWMODEL_API FMVVMViewClass_CompiledBinding
{
	GENERATED_BODY()

	friend UE::MVVM::Private::FMVVMViewBlueprintCompiler;

public:
	/** @return The id for the FieldId on the source (if forward) or the destination (if backward). */
	FMVVMVCompiledFieldId GetSourceFieldId() const
	{
		return FieldId;
	}

	/**
	 * @return The property name of the object that contains the SourceFieldId.
	 * It implements INotifyFieldValueChanged if it's not a One Time.
	 */
	FName GetSourceObjectPropertyName() const
	{
		return SourcePropertyName;
	}

	/** @return the binding. From source to destination (if forward) or from destination to source (if backward). */
	const FMVVMVCompiledBinding& GetBinding() const
	{
		return Binding;
	}

	/** @return true if this Binding should be executed at initialization. */
	bool NeedsExecutionAtInitialization() const
	{
		return (Flags & EBindingFlags::ForwardBinding) != 0;
	}

	/** @return true if this Binding should be executed once (at initialization) but should not be executed when the SourceFieldId value changes. */
	bool IsOneTime() const
	{
		return (Flags & EBindingFlags::OneTime) != 0;
	}

	/** @return true if the binding is enabled by default. */
	bool IsEnabledByDefault() const
	{
		return (Flags & EBindingFlags::EnabledByDefault) != 0;
	}

	/** @return true if it's normal that the binding could not find it's source when registering it. */
	bool IsRegistrationOptional() const
	{
		return (Flags & EBindingFlags::ViewModelOptional) != 0;
	}

	/** @return true if the binding use a conversion function and that the conversion function is complex. */
	bool IsConversionFunctionComplex() const
	{
		return (Flags & EBindingFlags::ConversionFunctionIsComplex) != 0;
	}

	/** @return a human readable version of the binding that can be use for debugging purposes. */
	FString ToString() const;

private:
	UPROPERTY()
	FMVVMVCompiledFieldId FieldId;

	UPROPERTY()
	FName SourcePropertyName;

	UPROPERTY()
	FMVVMVCompiledBinding Binding;

	/** How the binding should be executed. */
	UPROPERTY()
	EMVVMViewBindingUpdateMode UpdateMode = EMVVMViewBindingUpdateMode::Immediate;

	enum EBindingFlags
	{
		None = 0,
		ForwardBinding = 1 << 0, // True when the binding goes from Source to Destination.
		TwoWayBinding = 1 << 1, // The binding is one part of a 2 ways binding.
		OneTime = 1 << 2,
		EnabledByDefault = 1 << 3,
		ViewModelOptional = 1 << 4,	// The source (viewmodel) can be nullptr and the binding could failed and should not log a warning.
		ConversionFunctionIsComplex = 1 << 5,	// The conversion function is complex, there is no input. The inputs are calculated in the BP function.
		Unused01 = 1 << 6,
		Unused02 = 1 << 7,
	};

	UPROPERTY()
	uint8 Flags = EBindingFlags::None;
};


/**
 * Shared between every instances of the same View class.
 */
UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMViewClass : public UWidgetBlueprintGeneratedClassExtension
{
	GENERATED_BODY()

	friend UE::MVVM::Private::FMVVMViewBlueprintCompiler;

public:
	//~ Begin UWidgetBlueprintGeneratedClassExtension
	virtual void Initialize(UUserWidget* UserWidget) override;
	//~ End UWidgetBlueprintGeneratedClassExtension

public:
	/** Get the list of the needed ViewModel. */
	const TArrayView<const FMVVMViewClass_SourceCreator> GetViewModelCreators() const
	{
		return MakeArrayView(SourceCreators);
	}

	/** Get the container of all the bindings. */
	const FMVVMCompiledBindingLibrary& GetBindingLibrary() const
	{
		return BindingLibrary;
	}

	/**  */
	const TArrayView<const FMVVMViewClass_CompiledBinding> GetCompiledBindings() const
	{
		return MakeArrayView(CompiledBindings);
	}

	/** */
	const FMVVMViewClass_CompiledBinding& GetCompiledBinding(int32 Index) const
	{
		return CompiledBindings[Index];
	}

private:
	/**  */
	TArrayView<FMVVMViewClass_CompiledBinding> GetCompiledBindings()
	{
		return MakeArrayView(CompiledBindings);
	}

private:
	/** Data to retrieve/create the sources (could be viewmodel, widget, ...). */
	UPROPERTY()
	TArray<FMVVMViewClass_SourceCreator> SourceCreators;

	/** */
	UPROPERTY()
	TArray<FMVVMViewClass_CompiledBinding> CompiledBindings;

	/** All the bindings shared between all the View instance. */
	UPROPERTY()
	FMVVMCompiledBindingLibrary BindingLibrary;

	/** */
	bool bLoaded = false;
};
