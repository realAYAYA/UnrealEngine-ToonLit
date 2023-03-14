// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "View/MVVMView.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"

#include "MVVMBlueprintView.generated.h"

class UWidget;
class UWidgetBlueprint;

/**
 * 
 */
UCLASS(Within=MVVMWidgetBlueprintExtension_View)
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMBlueprintView : public UObject
{
	GENERATED_BODY()

public:
	FMVVMBlueprintViewModelContext* FindViewModel(FGuid ViewModelId);
	const FMVVMBlueprintViewModelContext* FindViewModel(FGuid ViewModelId) const;
	const FMVVMBlueprintViewModelContext* FindViewModel(FName ViewModelName) const;

	void AddViewModel(const FMVVMBlueprintViewModelContext& NewContext);
	bool RemoveViewModel(FGuid ViewModelId);
	int32 RemoveViewModels(const TArrayView<FGuid> ViewModelIds);
	bool RenameViewModel(FName OldViewModelName, FName NewViewModelName);
	void SetViewModels(const TArray<FMVVMBlueprintViewModelContext>& ViewModelContexts);

	const TArrayView<const FMVVMBlueprintViewModelContext> GetViewModels() const
	{
		return AvailableViewModels; 
	}

	const FMVVMBlueprintViewBinding* FindBinding(const UWidget* Widget, const FProperty* Property) const;
	FMVVMBlueprintViewBinding* FindBinding(const UWidget* Widget, const FProperty* Property);

	void RemoveBinding(const FMVVMBlueprintViewBinding* Binding);
	void RemoveBindingAt(int32 Index);

	FMVVMBlueprintViewBinding& AddBinding(const UWidget* Widget, const FProperty* Property);
	FMVVMBlueprintViewBinding& AddDefaultBinding();

	int32 GetNumBindings() const
	{
		return Bindings.Num();
	}

	FMVVMBlueprintViewBinding* GetBindingAt(int32 Index);
	const FMVVMBlueprintViewBinding* GetBindingAt(int32 Index) const;

	TArrayView<FMVVMBlueprintViewBinding> GetBindings()
	{
		return Bindings;
	}

	const TArrayView<const FMVVMBlueprintViewBinding> GetBindings() const
	{
		return Bindings;
	}

#if WITH_EDITOR
//	virtual void PostEditUndo() override;

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChainEvent) override;

	void WidgetRenamed(FName OldObjectName, FName NewObjectName);
#endif

	virtual void PostLoad() override;

	DECLARE_EVENT(UMVVMBlueprintView, FOnBindingsUpdated);
	FOnBindingsUpdated OnBindingsUpdated;

	DECLARE_EVENT(UMVVMBlueprintView, FOnViewModelsUpdated);
	FOnViewModelsUpdated OnViewModelsUpdated;

	// Use during compilation to clean the automatically generated graph.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEdGraph>> TemporaryGraph;

private:
	UPROPERTY(EditAnywhere, Category = "MVVM")
	TArray<FMVVMBlueprintViewBinding> Bindings;

	UPROPERTY(EditAnywhere, Category = "MVVM")
	TArray<FMVVMBlueprintViewModelContext> AvailableViewModels;
};
