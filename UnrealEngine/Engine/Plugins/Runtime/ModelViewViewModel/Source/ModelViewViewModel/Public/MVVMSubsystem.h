// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "Types/MVVMFieldVariant.h"
#include "Types/MVVMBindingMode.h"
#include "Types/MVVMViewModelCollection.h"

#include "UObject/Package.h"
#include "MVVMSubsystem.generated.h"

struct FMVVMAvailableBinding;
struct FMVVMBindingName;
template <typename ValueType, typename ErrorType> class TValueOrError;

class UMVVMView;
class UMVVMViewModelBase;
class UUserWidget;
class UWidget;
class UWidgetTree;

/** */
UCLASS(DisplayName="Viewmodel Engine Subsytem")
class MODELVIEWVIEWMODEL_API UMVVMSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin UEngineSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End UEngineSubsystem interface

	UFUNCTION(BlueprintCallable, Category = "Viewmodel", meta = (DisplayName = "Get View From User Widget"))
	UMVVMView* K2_GetViewFromUserWidget(const UUserWidget* UserWidget) const;

	static UMVVMView* GetViewFromUserWidget(const UUserWidget* UserWidget);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	bool DoesWidgetTreeContainedWidget(const UWidgetTree* WidgetTree, const UWidget* ViewWidget) const;

	/** @return The list of all the AvailableBindings that are available for the Class. */
	UFUNCTION(BlueprintCallable, Category = "Viewmodel", meta = (DisplayName = "Get Available Bindings"))
	TArray<FMVVMAvailableBinding> K2_GetAvailableBindings(const UClass* Class, const UClass* Accessor) const;

	static TArray<FMVVMAvailableBinding> GetAvailableBindings(const UClass* Class, const UClass* Accessor);

	/**
	 * @return The list of all the AvailableBindings that are available from the SriptStuct.
	 * @note When FMVVMAvailableBinding::HasNotify is false, a notification can still be triggered by the owner of the struct. The struct changed but which property of the struct changed is unknown.
	 */
	static TArray<FMVVMAvailableBinding> GetAvailableBindingsForStruct(const UScriptStruct* Struct);

	static TArray<FMVVMAvailableBinding> GetAvailableBindingsForEvent(const UClass* Class, const UClass* Accessor);

	/** @return The AvailableBinding from a BindingName. */
	UFUNCTION(BlueprintCallable, Category = "Viewmodel", meta = (DisplayName = "Get Available Binding"))
	FMVVMAvailableBinding K2_GetAvailableBinding(const UClass* Class, FMVVMBindingName BindingName, const UClass* Accessor) const;

	static FMVVMAvailableBinding GetAvailableBinding(const UClass* Class, FMVVMBindingName BindingName, const UClass* Accessor);

	/** @return The AvailableBinding from a field. */
	static FMVVMAvailableBinding GetAvailableBindingForField(UE::MVVM::FMVVMConstFieldVariant Variant, const UClass* Accessor);

	static FMVVMAvailableBinding GetAvailableBindingForEvent(UE::MVVM::FMVVMConstFieldVariant FieldVariant, const UClass* Accessor);

	static FMVVMAvailableBinding GetAvailableBindingForEvent(const UClass* Class, FMVVMBindingName BindingName, const UClass* Accessor);

	UE_DEPRECATED(5.3, "GetGlobalViewModelCollection has been deprecated, please use the game instance subsystem.")
	UFUNCTION(BlueprintCallable, Category = "Viewmodel", meta=(DeprecatedFunction, DeprecatedMessage = "This version of GetGlobalViewModelCollection has been deprecated, please use GetGlobalViewModelCollection from the Viewmodel Game subsystem."))
	UMVVMViewModelCollectionObject* GetGlobalViewModelCollection() const;

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

	[[nodiscard]] TValueOrError<bool, FText> IsBindingValid(FConstDirectionalBindingArgs Args) const;
	[[nodiscard]] TValueOrError<bool, FText> IsBindingValid(FDirectionalBindingArgs Args) const;
	[[nodiscard]] TValueOrError<bool, FText> IsBindingValid(FBindingArgs Args) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Templates/ValueOrError.h"
#include "Types/MVVMBindingName.h"
#endif
