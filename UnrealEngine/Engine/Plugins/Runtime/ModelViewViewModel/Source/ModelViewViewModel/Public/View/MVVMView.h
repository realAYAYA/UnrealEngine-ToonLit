// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"
#include "UObject/Object.h"

#include "Extensions/UserWidgetExtension.h"
#include "FieldNotification/FieldId.h"
#include "FieldNotification/IFieldValueChanged.h"
#include "Types/MVVMBindingName.h"

#include "MVVMView.generated.h"

class UMVVMViewClass;
struct FMVVMViewClass_CompiledBinding;
class UMVVMViewModelBase;
class UWidget;


/**
 * Instance UMVVMClassExtension_View for the UUserWdiget
 */
UCLASS(Transient, DisplayName="MVVM View")
class MODELVIEWVIEWMODEL_API UMVVMView : public UUserWidgetExtension
{
	GENERATED_BODY()

public:
	void ConstructView(const UMVVMViewClass* ClassExtension);

	//~ Begin UUserWidgetExtension implementation
	virtual void Construct() override;
	virtual void Destruct() override;

// todo needed when we will support different type of update
	//virtual bool RequiresTick() const override;
	//virtual void Tick(const FGeometry& MyGeometry, float InDeltaTime) override;
	//~ End UUserWidgetExtension implementation

// todo a way to identify a binding from outside. maybe a unique name in the editor?
	//UFUNCTION(BlueprintCallable, Category = "MVVM")
	//void SetLibraryBindingEnabled(FGuid ViewModelId, FMVVMBindingName BindingName, bool bEnable);

	//UFUNCTION(BlueprintCallable, Category = "MVVM")
	//bool IsLibraryBindingEnabled(FGuid ViewModelId, FMVVMBindingName BindingName) const;

	UFUNCTION(BlueprintCallable, Category = "MVVM")
	bool SetViewModel(FName ViewModelName, UMVVMViewModelBase* ViewModel);

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
};
