// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Widgets/SCompoundWidget.h"
#include "Input/Reply.h"
#include "MVVMBlueprintViewBinding.h"
#include "MVVMBlueprintViewModelContext.h"

class ITableRow;
template<typename T>
class SListView;
class SMVVMViewModelContextListWidget;
template<typename T>
class STreeView;
class STableViewBase;
class UMVVMViewModelBase;
class UUserWidget;
class UWidgetBlueprint;

namespace UE::MVVM
{
	struct FMVVMFieldVariant;
	class SSourceBindingList;

	namespace Private
	{
		class FMVVMViewModelTreeNode;
	}
}

DECLARE_DELEGATE_OneParam(FOnViewModelContextsPicked, TArray<FMVVMBlueprintViewModelContext>);

class SMVVMManageViewModelsWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMVVMManageViewModelsWidget) {}
		SLATE_NAMED_SLOT(FArguments, ButtonsPanel)
		SLATE_ARGUMENT(bool, bUseDefaultButtons)
		SLATE_ARGUMENT(TWeakPtr<SWindow>, ParentWindow)
		SLATE_ARGUMENT(FOnViewModelContextsPicked, OnViewModelContextsPickedDelegate)
		SLATE_ARGUMENT(UWidgetBlueprint*, WidgetBlueprint)
	SLATE_END_ARGS()

	enum class EViewMode : uint8
	{
		List,
		Hierarchy,
		Tag
	};

public:
	void Construct(const FArguments& InArgs);

private:
	typedef UE::MVVM::Private::FMVVMViewModelTreeNode FMVVMViewModelTreeNode;

	void PopulateViewModelsTree();

	void SetViewMode(EViewMode Mode);

	TSharedRef<SWidget> GenerateSearchBar();

	void UpdateSearchString(const FString& SearchString);

	void RefreshTreeView(bool bForceExpandAll = false);
	void RebuildTreeViewSource(TArray<TSharedPtr<FMVVMViewModelTreeNode>>& ViewSource, TSharedRef<FMVVMViewModelTreeNode> RootNode);
	static void RebuildTreeViewSource_Helper(TSharedRef<FMVVMViewModelTreeNode>& Parent, TSharedRef<FMVVMViewModelTreeNode>& CloneParent, const TSet<TSharedRef<FMVVMViewModelTreeNode>>& NodesToDiscard);

	void SetAllExpansionStates_Helper(TSharedPtr<FMVVMViewModelTreeNode> InNode, bool bInExpansionState);
	void RestoreExpansionStatesInTree();
	static bool RestoreExpansionStatesInTree_Helper(TSharedRef<FMVVMViewModelTreeNode> InNode, TMap<FString, bool>& ExpansionStateMap, SMVVMManageViewModelsWidget* Self);

	TSharedRef<SWidget> HandleGetMenuContent_ViewSettings();
	void HandleGetChildrenForTreeView(TSharedPtr<FMVVMViewModelTreeNode> InParent, TArray<TSharedPtr<FMVVMViewModelTreeNode>>& OutChildren);
	TSharedRef<ITableRow> HandleGenerateRowForTreeView(TSharedPtr<FMVVMViewModelTreeNode> Item, const TSharedRef<STableViewBase>& OwnerTable);
	void HandleTreeViewExpansionChanged(TSharedPtr<FMVVMViewModelTreeNode> Item, bool bExpanded);
	void HandleTreeRowDoubleClicked(TSharedPtr<FMVVMViewModelTreeNode> Item);
	void HandleTreeViewSelectionChanged(TSharedPtr<FMVVMViewModelTreeNode> Item, ESelectInfo::Type SelectInfo);

	FReply HandleClicked_AddViewModel(TSharedPtr<FMVVMViewModelTreeNode> InNode);
	FReply HandleClicked_Finish();
	FReply HandleClicked_Cancel();

	bool IsFinishEnabled() const;

	TSubclassOf<UMVVMViewModelBase> GetClassFromNode(TSharedRef<FMVVMViewModelTreeNode> ClassNode);

private:
	TSharedPtr<FMVVMViewModelTreeNode> ClassListRootNode;
	TSharedPtr<FMVVMViewModelTreeNode> ClassTreeRootNode;
	TSharedPtr<FMVVMViewModelTreeNode> TagTreeRootNode;

	TSharedPtr<FMVVMViewModelTreeNode> CurrentViewTreeRootNode;

	TArray<TSharedPtr<FMVVMViewModelTreeNode>> NodeTreeViewSource;

	TMap<FString, bool> ClassTreeExpansionStateMap;
	TMap<FString, bool> TagTreeExpansionStateMap;

	TSharedPtr<STreeView<TSharedPtr<FMVVMViewModelTreeNode>>> TreeViewWidget;

	EViewMode CurrentViewMode = EViewMode::List;

	FOnViewModelContextsPicked OnViewModelContextsPicked;

	TWeakPtr<SWindow> WeakParentWindow;

	TSharedPtr<UE::MVVM::SSourceBindingList> BindingListWidget;

	bool bSearchStringIsEmpty = true;
	UUserWidget* OuterWidget = nullptr;

	TSharedPtr<SMVVMViewModelContextListWidget> ViewModelContextListWidget;
};
