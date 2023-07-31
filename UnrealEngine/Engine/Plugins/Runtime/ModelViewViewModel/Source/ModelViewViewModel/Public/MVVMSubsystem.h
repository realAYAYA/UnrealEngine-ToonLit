// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "Templates/ValueOrError.h"
#include "Types/MVVMBindingName.h"
#include "Types/MVVMFieldVariant.h"
#include "Types/MVVMBindingMode.h"
#include "Types/MVVMViewModelCollection.h"

#include "MVVMSubsystem.generated.h"

class UMVVMView;
class UMVVMViewModelBase;
class UUserWidget;
class UWidget;
class UWidgetTree;

/** */
UCLASS()
class MODELVIEWVIEWMODEL_API UMVVMSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UEngineSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UEngineSubsystem interface

	UFUNCTION(BlueprintCallable, Category="MVVM")
	UMVVMView* GetViewFromUserWidget(const UUserWidget* UserWidget) const;

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	bool DoesWidgetTreeContainedWidget(const UWidgetTree* WidgetTree, const UWidget* ViewWidget) const;

	/**
	 * @return The list of all the bindings that are available for the Class.
	 */
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	TArray<FMVVMAvailableBinding> GetAvailableBindings(const UClass* Class, const UClass* Accessor) const;

	/**
	 * @return The list of all the bindings that are available from the SriptStuct.
	 * @note When FMVVMAvailableBinding::HasNotify is false, a notification can still be triggered by the owner of the struct. The struct changed but which property of the struct changed is unknown.
	 */
	TArray<FMVVMAvailableBinding> GetAvailableBindingsForStruct(const UScriptStruct* Struct) const;

	/**
	 * @return the available binding from the binding.
	 */
	UFUNCTION(BlueprintCallable, Category = "MVVM")
	FMVVMAvailableBinding GetAvailableBinding(const UClass* Class, FMVVMBindingName BindingName, const UClass* Accessor) const;

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	UMVVMViewModelCollectionObject* GetGlobalViewModelCollection() const
	{
		return GlobalViewModelCollection;
	}

	struct FConstDirectionalBindingArgs
	{
		UE::MVVM::FMVVMConstFieldVariant SourceBinding;
		UE::MVVM::FMVVMConstFieldVariant DestinationBinding;
		const UFunction* ConversionFunction = nullptr;
	};

	struct FDirectionalBindingArgs
	{
		UE::MVVM::FMVVMFieldVariant SourceBinding;
		UE::MVVM::FMVVMFieldVariant DestinationBinding;
		UFunction* ConversionFunction = nullptr;

		FConstDirectionalBindingArgs ToConst() const
		{
			FConstDirectionalBindingArgs ConstArgs;
			ConstArgs.SourceBinding = SourceBinding;
			ConstArgs.DestinationBinding = DestinationBinding;
			ConstArgs.ConversionFunction = ConversionFunction;
			return MoveTemp(ConstArgs);
		}
	};

	struct FBindingArgs
	{
		EMVVMBindingMode Mode = EMVVMBindingMode::OneWayToDestination;
		FDirectionalBindingArgs ForwardArgs;
		FDirectionalBindingArgs BackwardArgs;
	};

	UE_NODISCARD TValueOrError<bool, FText> IsBindingValid(FConstDirectionalBindingArgs Args) const;
	UE_NODISCARD TValueOrError<bool, FText> IsBindingValid(FDirectionalBindingArgs Args) const;
	UE_NODISCARD TValueOrError<bool, FText> IsBindingValid(FBindingArgs Args) const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UMVVMViewModelCollectionObject> GlobalViewModelCollection;
};
