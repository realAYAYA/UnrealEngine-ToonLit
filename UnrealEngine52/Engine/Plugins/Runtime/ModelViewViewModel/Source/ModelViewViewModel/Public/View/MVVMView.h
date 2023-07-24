// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once


#include "Extensions/UserWidgetExtension.h"

#include "MVVMView.generated.h"

class INotifyFieldValueChanged;
namespace UE::FieldNotification { struct FFieldId; }
template <typename InterfaceType> class TScriptInterface;

class UMVVMViewClass;
struct FMVVMViewClass_CompiledBinding;
struct FMVVMViewDelayedBinding;
class UMVVMViewModelBase;
class UWidget;
namespace UE::MVVM
{
	class FDebugging;
}

/**
 * Instance UMVVMClassExtension_View for the UUserWdiget
 */
UCLASS(Transient, DisplayName="MVVM View")
class MODELVIEWVIEWMODEL_API UMVVMView : public UUserWidgetExtension
{
	GENERATED_BODY()

	friend UE::MVVM::FDebugging;

public:
	void ConstructView(const UMVVMViewClass* ClassExtension);

	//~ Begin UUserWidgetExtension implementation
	virtual void Construct() override;
	virtual void Destruct() override;
	//~ End UUserWidgetExtension implementation

	const UMVVMViewClass* GetViewClass() const
	{
		return ClassExtension;
	}

	void ExecuteDelayedBinding(const FMVVMViewDelayedBinding& DelayedBinding) const;
	void ExecuteEveryTickBindings() const;

// todo a way to identify a binding from outside. maybe a unique name in the editor?
	//UFUNCTION(BlueprintCallable, Category = "MVVM")
	//void SetLibraryBindingEnabled(FGuid ViewModelId, FMVVMBindingName BindingName, bool bEnable);

	//UFUNCTION(BlueprintCallable, Category = "MVVM")
	//bool IsLibraryBindingEnabled(FGuid ViewModelId, FMVVMBindingName BindingName) const;

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	bool SetViewModel(FName ViewModelName, TScriptInterface<INotifyFieldValueChanged> ViewModel);

private:
	void HandledLibraryBindingValueChanged(UObject* InViewModel, UE::FieldNotification::FFieldId InFieldId, int32 InCompiledBindingIndex) const;

	void ExecuteLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding) const;
	void ExecuteLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding, UObject* Source) const;

	void EnableLibraryBinding(const FMVVMViewClass_CompiledBinding& Item, int32 BindingIndex);
	void DisableLibraryBinding(const FMVVMViewClass_CompiledBinding& Item, int32 BindingIndex);
	bool IsLibraryBindingEnabled(int32 InBindindIndex) const;

	bool RegisterLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding, int32 BindingIndex);
	void UnregisterLibraryBinding(const FMVVMViewClass_CompiledBinding& Binding);

	TScriptInterface<INotifyFieldValueChanged> FindSource(const FMVVMViewClass_CompiledBinding& Binding, bool bAllowNull) const;

private:
// todo support dynamic runtime binding.
	/** Binding that are added dynamically at runtime. */
	//TArray<FMVVMView_Binding> RegisteredDynamicBindings;

	struct FRegisteredSource
	{
		TWeakObjectPtr<UObject> Source;
		int32 Count = 0;
	};

	UPROPERTY(Transient)
	TObjectPtr<const UMVVMViewClass> ClassExtension;

	/** A list of all source and the amount of registered binding. */
	TArray<FRegisteredSource> AllSources;

	/** The binding that are enabled for the instance. */
	TBitArray<> EnabledLibraryBindings;

	/** Should log when a binding is executed. */
	bool bLogBinding = false;

	/** Is the Construct method was called. */
	bool bConstructed = false;

	/** Is the Construct method was called. */
	bool bHasEveryTickBinding = false;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "FieldNotification/FieldId.h"
#include "FieldNotification/IFieldValueChanged.h"
#include "Templates/SubclassOf.h"
#include "Types/MVVMBindingName.h"
#endif
