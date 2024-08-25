// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorSubsystem.h"
#include "MVVMBlueprintPin.h"
#include "UObject/Package.h"

#include "MVVMEditorSubsystem.generated.h"

class UEdGraphPin;
class UMVVMBlueprintView;
enum class EMVVMBindingMode : uint8;
enum class EMVVMExecutionMode : uint8;
namespace UE::MVVM { struct FBindingSource; }
struct FMVVMAvailableBinding;
struct FMVVMBlueprintFunctionReference;
struct FMVVMBlueprintPropertyPath;
struct FMVVMBlueprintViewBinding;
template <typename T> class TSubclassOf;

class UEdGraph;
class UK2Node;
class UK2Node_CallFunction;
class UMVVMBlueprintViewEvent;
class UWidgetBlueprint;

/** */
UCLASS(DisplayName="Viewmodel Editor Subsystem")
class MODELVIEWVIEWMODELEDITOR_API UMVVMEditorSubsystem : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	UMVVMBlueprintView* RequestView(UWidgetBlueprint* WidgetBlueprint) const;

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	UMVVMBlueprintView* GetView(const UWidgetBlueprint* WidgetBlueprint) const;

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	FGuid AddViewModel(UWidgetBlueprint* WidgetBlueprint, const UClass* ViewModel);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	FGuid AddInstancedViewModel(UWidgetBlueprint* WidgetBlueprint);
	
	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	void RemoveViewModel(UWidgetBlueprint* WidgetBlueprint, FName ViewModel);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	bool VerifyViewModelRename(UWidgetBlueprint* WidgetBlueprint, FName ViewModel, FName NewViewModel, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	bool RenameViewModel(UWidgetBlueprint* WidgetBlueprint, FName ViewModel, FName NewViewModel, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	bool ReparentViewModel(UWidgetBlueprint* WidgetBlueprint, FName ViewModel, const UClass* NewViewModel, FText& OutError);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	FMVVMBlueprintViewBinding& AddBinding(UWidgetBlueprint* WidgetBlueprint);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	void RemoveBinding(UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	UMVVMBlueprintViewEvent* AddEvent(UWidgetBlueprint* WidgetBlueprint);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	void RemoveEvent(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	TArray<FMVVMAvailableBinding> GetChildViewModels(TSubclassOf<UObject> Class, TSubclassOf<UObject> Accessor);

	UE_DEPRECATED(5.4, "SetSourceToDestinationConversionFunction with a UFunction is deprecated.")
	void SetSourceToDestinationConversionFunction(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const UFunction* ConversionFunction);
	void SetSourceToDestinationConversionFunction(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FMVVMBlueprintFunctionReference ConversionFunction);
	UE_DEPRECATED(5.4, "SetDestinationToSourceConversionFunction  with a UFunction is deprecated.")
	void SetDestinationToSourceConversionFunction(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const UFunction* ConversionFunction);
	void SetDestinationToSourceConversionFunction(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FMVVMBlueprintFunctionReference ConversionFunction);
	void SetDestinationPathForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FMVVMBlueprintPropertyPath Field);
	void SetSourcePathForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, FMVVMBlueprintPropertyPath Field);
	void OverrideExecutionModeForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, EMVVMExecutionMode Mode);
	void ResetExecutionModeForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding);
	void SetBindingTypeForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, EMVVMBindingMode Type);
	void SetEnabledForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, bool bEnabled);
	void SetCompileForBinding(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, bool bCompile);

	void SetEventPath(UMVVMBlueprintViewEvent* Event, FMVVMBlueprintPropertyPath PropertyPath);
	void SetEventDestinationPath(UMVVMBlueprintViewEvent* Event, FMVVMBlueprintPropertyPath PropertyPath);
	void SetEventArgumentPath(UMVVMBlueprintViewEvent* Event, const FMVVMBlueprintPinId& PinId, const FMVVMBlueprintPropertyPath& PropertyPath) const;
	void SetEnabledForEvent(UMVVMBlueprintViewEvent* Event, bool bEnabled);
	void SetCompileForEvent(UMVVMBlueprintViewEvent* Event, bool bCompile);

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	bool IsValidConversionFunction(const UWidgetBlueprint* WidgetBlueprint, const UFunction* Function, const FMVVMBlueprintPropertyPath& Source, const FMVVMBlueprintPropertyPath& Destination) const;
	bool IsValidConversionNode(const UWidgetBlueprint* WidgetBlueprint, const TSubclassOf<UK2Node> Function, const FMVVMBlueprintPropertyPath& Source, const FMVVMBlueprintPropertyPath& Destination) const;

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	bool IsSimpleConversionFunctionA(const UFunction* Function) const;

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	UEdGraph* GetConversionFunctionGraph(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination) const;

	UE_DEPRECATED(5.4, "GetConversionFunction was moved to MVVMBlueprintViewConversionFunction.")
	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	UFunction* GetConversionFunction(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination) const;

	UE_DEPRECATED(5.3, "GetConversionFunctionNode was moved to MVVMBlueprintViewConversionFunction.")
	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	UK2Node_CallFunction* GetConversionFunctionNode(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, bool bSourceToDestination) const;

	UFUNCTION(BlueprintCallable, Category = "Viewmodel")
	TArray<UFunction*> GetAvailableConversionFunctions(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintPropertyPath& Source, const FMVVMBlueprintPropertyPath& Destination) const;

	FMVVMBlueprintPropertyPath GetPathForConversionFunctionArgument(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;
	void SetPathForConversionFunctionArgument(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, const FMVVMBlueprintPropertyPath& Path, bool bSourceToDestination) const;
	UEdGraphPin* GetConversionFunctionArgumentPin(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;

	void SplitPin(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;
	bool CanSplitPin(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;
	void SplitPin(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event, const FMVVMBlueprintPinId& PinId) const;
	bool CanSplitPin(const UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event, const FMVVMBlueprintPinId& PinId) const;
	void RecombinePin(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;
	bool CanRecombinePin(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;
	void RecombinePin(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event, const FMVVMBlueprintPinId& PinId) const;
	bool CanRecombinePin(const UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event, const FMVVMBlueprintPinId& PinId) const;
	void ResetPinToDefaultValue(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding,const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;
	bool CanResetPinToDefaultValue(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;
	void ResetPinToDefaultValue(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event, const FMVVMBlueprintPinId& PinId) const;
	bool CanResetPinToDefaultValue(const UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event, const FMVVMBlueprintPinId& PinId) const;
	void ResetOrphanedPin(UWidgetBlueprint* WidgetBlueprint, FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;
	bool CanResetOrphanedPin(const UWidgetBlueprint* WidgetBlueprint, const FMVVMBlueprintViewBinding& Binding, const FMVVMBlueprintPinId& PinId, bool bSourceToDestination) const;
	void ResetOrphanedPin(UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event,const FMVVMBlueprintPinId& PinId) const;
	bool CanResetOrphanedPin(const UWidgetBlueprint* WidgetBlueprint, UMVVMBlueprintViewEvent* Event, const FMVVMBlueprintPinId& PinId) const;

	TArray<UE::MVVM::FBindingSource> GetBindableWidgets(const UWidgetBlueprint* WidgetBlueprint) const;
	TArray<UE::MVVM::FBindingSource> GetAllViewModels(const UWidgetBlueprint* WidgetBlueprint) const;

	FGuid GetFirstBindingThatUsesViewModel(const UWidgetBlueprint* WidgetBlueprint, FGuid ViewModelId) const;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "MVVMBlueprintView.h"
#include "Templates/SubclassOf.h"
#include "Types/MVVMBindingMode.h"
#include "Types/MVVMBindingSource.h"
#include "Types/MVVMExecutionMode.h"
#include "Types/MVVMFieldVariant.h"
#endif
