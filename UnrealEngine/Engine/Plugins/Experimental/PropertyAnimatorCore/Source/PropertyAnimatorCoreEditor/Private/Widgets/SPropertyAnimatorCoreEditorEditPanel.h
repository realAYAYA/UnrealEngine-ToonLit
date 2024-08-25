// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Framework/Commands/UICommandList.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/PropertyAnimatorCoreEditorEditPanelOptions.h"
#include "Widgets/Views/PropertyAnimatorCoreEditorViewItem.h"
#include "Widgets/Views/STreeView.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/SCompoundWidget.h"

class SPropertyAnimatorCoreEditorControllersView;
class SPropertyAnimatorCoreEditorPropertiesView;
class UToolMenu;

/** Window content showing the link between controller and properties in various views */
class SPropertyAnimatorCoreEditorEditPanel : public SCompoundWidget
{
	friend class SPropertyAnimatorCoreEditorControllersViewTableRow;
	friend class SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow;

public:
	static inline constexpr int32 PropertiesViewIndex = 0;
	static inline constexpr int32 ControllersViewIndex = 1;
	static inline const FName HeaderPropertyColumnName = "Properties";
	static inline const FName HeaderAnimatorColumnName = "Animators";
	static inline const FName HeaderActionColumnName = "Actions";

	DECLARE_MULTICAST_DELEGATE(FOnGlobalSelectionChanged)
	FOnGlobalSelectionChanged OnGlobalSelectionChangedDelegate;

	DECLARE_MULTICAST_DELEGATE_OneParam(FOnControllerRenameRequested, UPropertyAnimatorCoreBase* /** Controller */)
	FOnControllerRenameRequested OnControllerRenameRequestedDelegate;

	SLATE_BEGIN_ARGS(SPropertyAnimatorCoreEditorEditPanel) {}
	SLATE_END_ARGS()

	virtual ~SPropertyAnimatorCoreEditorEditPanel() override;

	static TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> OpenWindow();

	void CloseWindow() const;

	void FocusWindow() const;

	void Construct(const FArguments& InArgs);

	void Update() const;

	FPropertyAnimatorCoreEditorEditPanelOptions& GetOptions()
	{
		return Options;
	}

	const FPropertyAnimatorCoreEditorEditPanelOptions& GetOptions() const
	{
		return Options;
	}

	TSet<FPropertiesViewControllerItem>& GetGlobalSelection()
	{
		return GlobalSelection;
	}

protected:
	FReply OnDragStart(const FGeometry& InGeometry, const FPointerEvent& InPointerEvent);
	virtual FReply OnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply OnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	TSharedRef<SWidget> GenerateContextMenuWidget();
	void FillAnimatorContextSection(UToolMenu* InToolMenu) const;

	void OnViewButtonClicked(ECheckBoxState InCheckBoxState, int32 InViewIdx);
	ECheckBoxState IsViewButtonActive(int32 InViewIdx) const;
	FText GetTitle() const;

	void OnControllerUpdated(UPropertyAnimatorCoreBase* InController);
	void OnControllerPropertyUpdated(UPropertyAnimatorCoreBase* InController, const FPropertyAnimatorCoreData& InProperty);

	bool CanExecuteDeleteCommand() const;
	void ExecuteDeleteCommand();

	bool CanExecuteRenameCommand() const;
	void ExecuteRenameCommand();

	/** Contains the available button views */
	TSharedPtr<SScrollBox> ViewsToolbar;

	/** Contains the different views widget */
	TSharedPtr<SWidgetSwitcher> ViewsSwitcher;

	/** Properties view widget */
	TSharedPtr<SPropertyAnimatorCoreEditorPropertiesView> PropertiesView;

	/** Controllers view widget */
	TSharedPtr<SPropertyAnimatorCoreEditorControllersView> ControllersView;

	int32 ActiveViewIndex = SPropertyAnimatorCoreEditorEditPanel::PropertiesViewIndex;

	FPropertyAnimatorCoreEditorEditPanelOptions Options;

	/** Selection on each views to apply commands */
	TSet<FPropertiesViewControllerItem> GlobalSelection;

	TSharedPtr<FUICommandList> CommandList;
};
