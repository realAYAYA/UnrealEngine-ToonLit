// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"

#include "MVVMBlueprintView.generated.h"

class UMVVMWidgetBlueprintExtension_View;

class UWidget;
class UWidgetBlueprint;

namespace  UE::MVVM
{
	enum class EBindingMessageType : uint8
	{
		Info,
		Warning,
		Error
	};

	struct FBindingMessage
	{
		FText MessageText;
		EBindingMessageType MessageType;
	};
}

/**
 *
 */
UCLASS(MinimalAPI)
class UMVVMBlueprintViewSettings : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Auto initialize the view sources when the Widget is constructed.
	 * If false, the user will have to initialize the sources manually.
	 * It prevents the sources evaluating until you are ready.
	 */
	UPROPERTY(EditAnywhere, Category = "Viewmodel")
	bool bInitializeSourcesOnConstruct = true;
	/**
	 * Auto initialize the view bindings when the Widget is constructed.
	 * If false, the user will have to initialize the bindings manually.
	 * It prevents bindings execution and improves performance when you know the widget won't be visible.
	 * @note All bindings are executed when the view is automatically initialized or manually initialized.
	 * @note Sources needs to be initialized before initializing the bindings.
	 * @note When Sources is manually initialized, the bindings will also be initialized if this is true.
	 */
	UPROPERTY(EditAnywhere, Category = "Viewmodel", meta=(EditCondition="bInitializeSourcesOnConstruct"))
	bool bInitializeBindingsOnConstruct = true;
};

/**
 * 
 */
UCLASS(Within=MVVMWidgetBlueprintExtension_View)
class MODELVIEWVIEWMODELBLUEPRINT_API UMVVMBlueprintView : public UObject
{
	GENERATED_BODY()

public:
	UMVVMBlueprintView();

public:
	UMVVMBlueprintViewSettings* GetSettings()
	{
		return Settings;
	}

	FMVVMBlueprintViewModelContext* FindViewModel(FGuid ViewModelId);
	const FMVVMBlueprintViewModelContext* FindViewModel(FGuid ViewModelId) const;
	const FMVVMBlueprintViewModelContext* FindViewModel(FName ViewModelName) const;

	void AddViewModel(const FMVVMBlueprintViewModelContext& NewContext);
	bool RemoveViewModel(FGuid ViewModelId);
	int32 RemoveViewModels(const TArrayView<FGuid> ViewModelIds);
	bool RenameViewModel(FName OldViewModelName, FName NewViewModelName);

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
	FMVVMBlueprintViewBinding* GetBinding(FGuid Id);
	const FMVVMBlueprintViewBinding* GetBinding(FGuid Id) const;

	TArrayView<FMVVMBlueprintViewBinding> GetBindings()
	{
		return Bindings;
	}

	const TArrayView<const FMVVMBlueprintViewBinding> GetBindings() const
	{
		return Bindings;
	}

	TArray<FText> GetBindingMessages(FGuid Id, UE::MVVM::EBindingMessageType InMessageType) const;
	bool HasBindingMessage(FGuid Id, UE::MVVM::EBindingMessageType InMessageType) const;
	void AddMessageToBinding(FGuid Id, UE::MVVM::FBindingMessage MessageToAdd);
	void ResetBindingMessages();

#if WITH_EDITOR
	virtual void PostLoad() override;
	virtual void PreSave(FObjectPreSaveContext Context) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChainEvent) override;

	void AddAssetTags(TArray<FAssetRegistryTag>& OutTags) const;
	void WidgetRenamed(FName OldObjectName, FName NewObjectName);
#endif

	virtual void Serialize(FArchive& Ar) override;

	DECLARE_EVENT(UMVVMBlueprintView, FOnBindingsUpdated);
	FOnBindingsUpdated OnBindingsUpdated;

	DECLARE_EVENT(UMVVMBlueprintView, FOnBindingsAdded);
	FOnBindingsAdded OnBindingsAdded;

	DECLARE_EVENT(UMVVMBlueprintView, FOnViewModelsUpdated);
	FOnViewModelsUpdated OnViewModelsUpdated;

	// Use during compilation to clean the automatically generated graph.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UEdGraph>> TemporaryGraph;

private:
	UPROPERTY()
	TObjectPtr<UMVVMBlueprintViewSettings> Settings;

	UPROPERTY(EditAnywhere, Category = "Viewmodel")
	TArray<FMVVMBlueprintViewBinding> Bindings;

	UPROPERTY(EditAnywhere, Category = "Viewmodel")
	TArray<FMVVMBlueprintViewModelContext> AvailableViewModels;

	TMap<FGuid, TArray<UE::MVVM::FBindingMessage>> BindingMessages;

	bool bIsContextSensitive;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "View/MVVMView.h"
#endif
