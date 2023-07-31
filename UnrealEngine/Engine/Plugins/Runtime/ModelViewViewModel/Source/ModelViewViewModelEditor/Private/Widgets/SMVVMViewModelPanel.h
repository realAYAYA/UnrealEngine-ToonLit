// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Misc/NotifyHook.h"
#include "Widgets/PropertyViewer/SPropertyViewer.h"
#include "Widgets/SMVVMViewModelBindingListWidget.h"
#include "Widgets/SCompoundWidget.h"

class FWidgetBlueprintEditor;
class SInlineEditableTextBlock;
class SPositiveActionButton;
class UBlueprintExtension;
class UMVVMBlueprintView;
class FUICommandList;
class IStructureDetailsView;

namespace UE::MVVM
{

/**
 *
 */
class SMVVMViewModelPanel : public SCompoundWidget, public FNotifyHook
{
public:
	SLATE_BEGIN_ARGS(SMVVMViewModelPanel) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<FWidgetBlueprintEditor> Editor);
	virtual ~SMVVMViewModelPanel();

	void OpenAddViewModelMenu();

private:
	void HandleViewUpdated(UBlueprintExtension* Extension);
	void HandleViewModelsUpdated();
	TSharedRef<SWidget> MakeAddMenu();
	void HandleCancelAddMenu();
	void HandleAddMenuViewModel(const UClass* SelectedClass);
	bool HandleCanEditViewmodelList() const;
	FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	TSharedRef<SWidget> HandleGenerateContainer(UE::PropertyViewer::SPropertyViewer::FHandle ContainerHandle, TOptional<FText> DisplayName);
	TSharedPtr<SWidget> HandleContextMenuOpening(UE::PropertyViewer::SPropertyViewer::FHandle ContainerHandle, TArrayView<const FFieldVariant> Field) const;
	void HandleSelectionChanged(UE::PropertyViewer::SPropertyViewer::FHandle, TArrayView<const FFieldVariant>, ESelectInfo::Type);
	bool HandleVerifyNameTextChanged(const FText& InText, FText& OutErrorMessage, FGuid ViewModelGuid);
	void HandleNameTextCommited(const FText& InText, ETextCommit::Type CommitInfo, FGuid ViewModelGuid);

	void HandleDeleteViewModel();
	bool HandleCanDeleteViewModel() const;
	void HandleRenameViewModel();
	bool HandleCanRenameViewModel() const;

	void CreateCommandList();
	void FillViewModel();
	bool RenameViewModelProperty(FGuid ViewModelGuid, const FText& RenameTo, bool bCommit, FText& OutErrorMessage) const;

	//~ Begin FNotifyHook
	virtual void NotifyPreChange(FProperty* PropertyAboutToChange) override;
	virtual void NotifyPostChange(const FPropertyChangedEvent& PropertyChangedEvent, FProperty* PropertyThatChanged) override;
	//~ End FNotifyHook

private:
	TSharedPtr<SPositiveActionButton> AddMenuButton;
	TSharedPtr<UE::PropertyViewer::SPropertyViewer> ViewModelTreeView;
	TUniquePtr<FFieldIterator_Bindable> FieldIterator;
	TSharedPtr<FUICommandList> CommandList;
	TSharedPtr<IStructureDetailsView> PropertyView;

	TMap<UE::PropertyViewer::SPropertyViewer::FHandle, FGuid> PropertyViewerHandles;
	TMap<FGuid, TSharedPtr<SInlineEditableTextBlock>> EditableTextBlocks;

	TWeakPtr<FWidgetBlueprintEditor> WeakBlueprintEditor;
	TWeakObjectPtr<UMVVMBlueprintView> WeakBlueprintView;
	FDelegateHandle ViewModelsUpdatedHandle;

	FGuid SelectedViewModelGuid;
	FName PreviousViewModelPropertyName;
};

} // namespace
