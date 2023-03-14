// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"
#include "Widgets/SCompoundWidget.h"

class ITableRow;
template<typename T>
class SListView;
class STableViewBase;
class UBlueprintGeneratedClass;
class UWidgetBlueprint;

DECLARE_DELEGATE_OneParam(FOnViewModelContextsUpdated, TArray<FMVVMBlueprintViewModelContext>);

class SMVVMViewModelContextListWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMVVMViewModelContextListWidget) {}
		SLATE_NAMED_SLOT(FArguments, ButtonsPanel)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, ParentWindow)
		SLATE_ARGUMENT(UWidgetBlueprint*, WidgetBlueprint)
		SLATE_ARGUMENT(TArrayView<const FMVVMBlueprintViewBinding>, Bindings)
		SLATE_ARGUMENT(TArray<FMVVMBlueprintViewModelContext>, ExistingViewModelContexts)
		SLATE_ARGUMENT(FOnViewModelContextsUpdated, OnViewModelContextsUpdated)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	void AddViewModelContext(TSubclassOf<UMVVMViewModelBase> ViewModelClass);
	TArray<FMVVMBlueprintViewModelContext> GetViewModelContexts() const;

private:
	TSharedRef<ITableRow> HandleGenerateRowForListView(TSharedPtr<FMVVMBlueprintViewModelContext> Item, const TSharedRef<STableViewBase>& OwnerTable);
	bool IsContextNameAvailable(FGuid Guid, FText ContextName);
	void RemoveViewModelContext(FGuid Guid);
	bool ValidateRemoveViewModelContext(TSharedPtr<FMVVMBlueprintViewModelContext> ContextToRemove);

	FReply HandleClicked_Finish();
	FReply HandleClicked_Cancel();

	TArray<TSharedPtr<FMVVMBlueprintViewModelContext>> ContextListSource;
	TSharedPtr<SListView<TSharedPtr<FMVVMBlueprintViewModelContext>>> ContextListWidget;
	TArray<FMVVMBlueprintViewModelContext> ExistingViewModelContexts;
	UWidgetBlueprint* WidgetBlueprint = nullptr;
	FOnViewModelContextsUpdated OnViewModelContextsUpdated;
	TArrayView<const FMVVMBlueprintViewBinding> Bindings;
	TWeakPtr<SWindow> WeakParentWindow;

	friend class SMVVMViewModelSelectionWidget;
};