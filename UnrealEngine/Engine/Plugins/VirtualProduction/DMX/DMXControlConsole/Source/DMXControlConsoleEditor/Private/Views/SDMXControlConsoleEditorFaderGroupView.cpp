// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXControlConsoleEditorFaderGroupView.h"

#include "DMXControlConsoleData.h"
#include "DMXControlConsoleEditorSelection.h"
#include "DMXControlConsoleFaderBase.h"
#include "DMXControlConsoleFaderGroup.h"
#include "DMXControlConsoleFaderGroupRow.h"
#include "DMXControlConsoleFixturePatchMatrixCell.h"
#include "Framework/Application/SlateApplication.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutBase.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutDefault.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutRow.h"
#include "Layouts/DMXControlConsoleEditorGlobalLayoutUser.h"
#include "Layouts/DMXControlConsoleEditorLayouts.h"
#include "Models/DMXControlConsoleEditorModel.h"
#include "ScopedTransaction.h"
#include "Style/DMXControlConsoleEditorStyle.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateColor.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SDMXControlConsoleEditorAddButton.h"
#include "Widgets/SDMXControlConsoleEditorExpandArrowButton.h"
#include "Widgets/SDMXControlConsoleEditorFader.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupPanel.h"
#include "Widgets/SDMXControlConsoleEditorFaderGroupToolbar.h"
#include "Widgets/SDMXControlConsoleEditorMatrixCell.h"


#define LOCTEXT_NAMESPACE "SDMXControlConsoleEditorFaderGroupView"

namespace UE::DMXControlConsoleEditor::DMXControlConsoleEditorFaderGroupView::Private
{
	static float CollapsedViewModeHeight = 280.f;
	static float ExpandedViewModeHeight = 360.f;
};

SDMXControlConsoleEditorFaderGroupView::SDMXControlConsoleEditorFaderGroupView()
	: ViewMode(EDMXControlConsoleEditorViewMode::Expanded)
{}

void SDMXControlConsoleEditorFaderGroupView::Construct(const FArguments& InArgs, const TObjectPtr<UDMXControlConsoleFaderGroup>& InFaderGroup)
{
	FaderGroup = InFaderGroup;

	if (!ensureMsgf(FaderGroup.IsValid(), TEXT("Invalid fader group, cannot create fader group view correctly.")))
	{
		return;
	}

	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	EditorConsoleModel->GetOnFaderGroupsViewModeChanged().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::OnViewModeChanged);
	EditorConsoleModel->GetOnControlConsoleForceRefresh().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::OnElementAdded);
	EditorConsoleModel->GetOnControlConsoleForceRefresh().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::OnElementRemoved);
	FaderGroup->GetOnFixturePatchChanged().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::OnFaderGroupFixturePatchChanged);
	FaderGroup->GetOnFaderGroupExpanded().AddSP(this, &SDMXControlConsoleEditorFaderGroupView::UpdateExpansionState);

	ChildSlot
		[
			SNew(SBorder)
			.BorderBackgroundColor(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderColor)
			.BorderImage(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderImage)
			[
				SNew(SBorder)
				.BorderBackgroundColor(FLinearColor(0.01f, 0.01f, 0.01f, 1.f))
				.BorderImage(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderImage)
				[
					SNew(SBorder)
					.BorderImage(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBackgroundBorderImage)
					.Padding(6.f)
					[
						SNew(SBox)
						.MinDesiredHeight(TAttribute<FOptionalSize>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewHeightByFadersViewMode))
						[
							SNew(SVerticalBox)
							// Toolbar section
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Left)
							.VAlign(VAlign_Top)
							.AutoHeight()
							[
								SAssignNew(FaderGroupToolbar, SDMXControlConsoleEditorFaderGroupToolbar, SharedThis(this))
								.OnAddFaderGroup(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroup)
								.OnAddFaderGroupRow(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupRow)
								.OnExpanded(this, &SDMXControlConsoleEditorFaderGroupView::OnExpandArrowClicked)
							]

							// Core section
							+ SVerticalBox::Slot()
							[
								SNew(SHorizontalBox)
								// Fader Group Core section
								+ SHorizontalBox::Slot()
								.Padding(20.f, 20.f, 8.f, 8.f)
								.MaxWidth(116.f)
								[
									SNew(SDMXControlConsoleEditorFaderGroupPanel, SharedThis(this))
									.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetViewModeVisibility, EDMXControlConsoleEditorViewMode::Collapsed))
								]

								// Add button section
								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								.MaxWidth(16.f)
								.AutoWidth()
								[
									SNew(SDMXControlConsoleEditorAddButton)
									.OnClicked(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupClicked)
									.ToolTipText(LOCTEXT("AddFaderGroupButton_ToolTip", "Add a new Fader Group next."))
									.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetAddButtonVisibility))
								]

								// Faders widget section
								+ SHorizontalBox::Slot()
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								.Padding(4.f, 2.f)
								.AutoWidth()
								[
									GenerateElementsWidget()
								]
							]

							// Add row button
							+ SVerticalBox::Slot()
							.HAlign(HAlign_Center)
							.VAlign(VAlign_Bottom)
							.AutoHeight()
							[
								SNew(SDMXControlConsoleEditorAddButton)
								.OnClicked(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupRowClicked)
								.ToolTipText(LOCTEXT("AddFaderGroupOnNewRowButton_ToolTip", "Add a new Fader Group on the next row."))
								.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetAddRowButtonVisibility))
							]
						]
					]
				]
			]
		];

	UpdateExpansionState();
}

int32 SDMXControlConsoleEditorFaderGroupView::GetIndex() const
{
	if (!FaderGroup.IsValid())
	{
		return INDEX_NONE;
	}

	return FaderGroup->GetIndex();
}

FString SDMXControlConsoleEditorFaderGroupView::GetFaderGroupName() const
{
	if (!FaderGroup.IsValid())
	{
		return FString();
	}

	return FaderGroup->GetFaderGroupName();
}

bool SDMXControlConsoleEditorFaderGroupView::CanAddFaderGroup() const
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* ControlConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return false;
	}

	// True if current Layout is User Layout, no vertical layout mode and no global filter
	const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = ControlConsoleLayouts->GetActiveLayout();
	return
		IsValid(CurrentLayout) &&
		CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass() &&
		CurrentLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Vertical &&
		ControlConsoleData->FilterString.IsEmpty();
}

bool SDMXControlConsoleEditorFaderGroupView::CanAddFaderGroupRow() const
{
	if (!FaderGroup.IsValid())
	{
		return false;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleData* ControlConsoleData = EditorConsoleModel->GetEditorConsoleData();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!ControlConsoleData || !ControlConsoleLayouts)
	{
		return false;
	}

	// True if current Layout is User Layout and there's no global filter
	const UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return false;
	}

	bool bCanAdd =
		CurrentLayout->GetClass() == UDMXControlConsoleEditorGlobalLayoutUser::StaticClass() &&
		CurrentLayout->GetLayoutMode() != EDMXControlConsoleLayoutMode::Horizontal &&
		ControlConsoleData->FilterString.IsEmpty();

	// True if grid layout mode and this is the first active fader group in the row
	if (CurrentLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Grid)
	{
		bCanAdd &= 
			FaderGroup->IsActive() &&
			CurrentLayout->GetFaderGroupColumnIndex(FaderGroup.Get()) == 0;
	}

	return bCanAdd;
}

bool SDMXControlConsoleEditorFaderGroupView::CanAddFader() const
{
	// True if fader group has no Fixture Patch and there's no global filter
	bool bCanAdd = FaderGroup.IsValid() && !FaderGroup->HasFixturePatch();

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	if (const UDMXControlConsoleData* ControlConsoleData = EditorConsoleModel->GetEditorConsoleData())
	{
		bCanAdd &= ControlConsoleData->FilterString.IsEmpty();
	}

	return bCanAdd;
}

FReply SDMXControlConsoleEditorFaderGroupView::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		if (FaderGroup.IsValid())
		{
			UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
			const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();

			if (MouseEvent.IsLeftShiftDown())
			{
				SelectionHandler->Multiselect(FaderGroup.Get());
			}
			else if (MouseEvent.IsControlDown())
			{
				if (IsSelected())
				{
					SelectionHandler->RemoveFromSelection(FaderGroup.Get());
				}
				else
				{
					SelectionHandler->AddToSelection(FaderGroup.Get());
				}
			}
			else
			{
				constexpr bool bNotifySelectionChange = false;
				SelectionHandler->ClearSelection(bNotifySelectionChange);
				SelectionHandler->AddToSelection(FaderGroup.Get());
			}
		}
	}

	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton && FaderGroupToolbar.IsValid())
	{
		const FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(AsShared(), WidgetPath, FaderGroupToolbar->GenerateSettingsMenuWidget(),
			FSlateApplication::Get().GetCursorPos(),
			FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));
	}

	return FReply::Handled();
}

FReply SDMXControlConsoleEditorFaderGroupView::OnMouseButtonDoubleClick(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton = GetExpandArrowButton();
		if (ExpandArrowButton.IsValid())
		{
			ExpandArrowButton->ToggleExpandArrow();

			return FReply::Handled();
		}
	}

	return FReply::Unhandled();
}

void SDMXControlConsoleEditorFaderGroupView::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	if (!ensureMsgf(FaderGroup.IsValid(), TEXT("Invalid fader group, cannot update fader group view state correctly.")))
	{
		return;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> AllElements = FaderGroup->GetElements();
	if (AllElements.Num() == ElementWidgets.Num())
	{
		return;
	}

	if (AllElements.Num() > ElementWidgets.Num())
	{
		OnElementAdded();
	}
	else
	{
		OnElementRemoved();
	}
}

TSharedRef<SWidget> SDMXControlConsoleEditorFaderGroupView::GenerateElementsWidget()
{
	TSharedRef<SWidget> ElementsWidget =
		SNew(SHorizontalBox)
		.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetElementsHorizontalBoxVisibility))

		//Add Faders Horizontal Box
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.AutoWidth()
		[
			SAssignNew(ElementsHorizontalBox, SHorizontalBox)
		]

		//Add Fader button
		+ SHorizontalBox::Slot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Top)
		.MaxWidth(20.f)
		.Padding(2.f, 4.f)
		.AutoWidth()
		[
			SNew(SDMXControlConsoleEditorAddButton)
			.OnClicked(this, &SDMXControlConsoleEditorFaderGroupView::OnAddFaderClicked)
			.ToolTipText(LOCTEXT("AddFaderButton_ToolTip", "Add a new Raw Fader."))
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetAddFaderButtonVisibility))
		];

	return ElementsWidget;
}

bool SDMXControlConsoleEditorFaderGroupView::IsSelected() const
{
	UDMXControlConsoleEditorModel* EditorConsoleModel = GetMutableDefault<UDMXControlConsoleEditorModel>();
	const TSharedRef<FDMXControlConsoleEditorSelection> SelectionHandler = EditorConsoleModel->GetSelectionHandler();
	return SelectionHandler->IsSelected(FaderGroup.Get());
}

TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> SDMXControlConsoleEditorFaderGroupView::GetExpandArrowButton() const
{
	return FaderGroupToolbar.IsValid() ? FaderGroupToolbar->GetExpandArrowButton() : nullptr;
}

void SDMXControlConsoleEditorFaderGroupView::OnElementAdded()
{
	if (!FaderGroup.IsValid())
	{
		return;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements = FaderGroup->GetElements();

	for (const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element : Elements)
	{
		if (!Element)
		{
			continue;
		}

		if (ContainsElement(Element))
		{
			continue;
		}

		AddElement(Element);
	}
}

void SDMXControlConsoleEditorFaderGroupView::AddElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
{
	if (!ensureMsgf(Element, TEXT("Invalid fader, cannot add new fader correctly.")))
	{
		return;
	}

	if (!ElementsHorizontalBox.IsValid())
	{
		return;
	}

	TSharedPtr<SWidget> ElementWidget = nullptr;
	UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = Cast<UDMXControlConsoleFixturePatchMatrixCell>(Element.GetObject());
	if (MatrixCell)
	{
		SAssignNew(ElementWidget, SDMXControlConsoleEditorMatrixCell, MatrixCell)
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetElementWidgetVisibility, MatrixCell));
	}
	else
	{
		UDMXControlConsoleFaderBase* Fader = Cast<UDMXControlConsoleFaderBase>(Element.GetObject());
		SAssignNew(ElementWidget, SDMXControlConsoleEditorFader, Fader)
			.Padding(FMargin(4.f, 0.f))
			.Visibility(TAttribute<EVisibility>::CreateSP(this, &SDMXControlConsoleEditorFaderGroupView::GetElementWidgetVisibility, Fader));
	}

	ElementWidgets.Add(ElementWidget);

	const int32 Index = Element->GetIndex();
	ElementsHorizontalBox->InsertSlot(Index)
		.HAlign(HAlign_Left)
		.AutoWidth()
		[
			ElementWidget.ToSharedRef()
		];
}

void SDMXControlConsoleEditorFaderGroupView::OnElementRemoved()
{
	if (!FaderGroup.IsValid())
	{
		return;
	}

	const TArray<TScriptInterface<IDMXControlConsoleFaderGroupElement>> Elements = FaderGroup->GetElements();

	TArray<TWeakPtr<SWidget>> ElementWidgetsToRemove;
	for (TWeakPtr<SWidget>& Widget : ElementWidgets)
	{
		if (!Widget.IsValid())
		{
			continue;
		}

		const TSharedPtr<SDMXControlConsoleEditorFader> FaderWidget = StaticCastSharedPtr<SDMXControlConsoleEditorFader>(Widget.Pin());
		if (FaderWidget.IsValid())
		{
			UDMXControlConsoleFaderBase* Fader = FaderWidget->GetFader();
			if (Fader && Elements.Contains(Fader))
			{
				continue;
			}
		}

		const TSharedPtr<SDMXControlConsoleEditorMatrixCell> MatrixCellWidget = StaticCastSharedPtr<SDMXControlConsoleEditorMatrixCell>(Widget.Pin());
		if (MatrixCellWidget.IsValid())
		{
			UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = MatrixCellWidget->GetMatrixCell();
			if (MatrixCell && Elements.Contains(MatrixCell))
			{
				continue;
			}
		}

		ElementsHorizontalBox->RemoveSlot(Widget.Pin().ToSharedRef());
		ElementWidgetsToRemove.Add(Widget);
	}

	ElementWidgets.RemoveAll([&ElementWidgetsToRemove](TWeakPtr<SWidget> FaderWidget)
		{
			return !FaderWidget.IsValid() || ElementWidgetsToRemove.Contains(FaderWidget);
		});
}

bool SDMXControlConsoleEditorFaderGroupView::ContainsElement(const TScriptInterface<IDMXControlConsoleFaderGroupElement>& Element)
{
	auto IsFaderInUseLambda = [Element](const TWeakPtr<SWidget> Widget)
	{
		if (!Widget.IsValid())
		{
			return false;
		}

		const TSharedPtr<SDMXControlConsoleEditorFader> FaderWidget = StaticCastSharedPtr<SDMXControlConsoleEditorFader>(Widget.Pin());
		if (FaderWidget.IsValid())
		{
			UDMXControlConsoleFaderBase* Fader = FaderWidget->GetFader();
			if (Fader)
			{
				return Fader == Element.GetObject();
			}
		}

		const TSharedPtr<SDMXControlConsoleEditorMatrixCell> MatrixCellWidget = StaticCastSharedPtr<SDMXControlConsoleEditorMatrixCell>(Widget.Pin());
		if (MatrixCellWidget.IsValid())
		{
			UDMXControlConsoleFixturePatchMatrixCell* MatrixCell = MatrixCellWidget->GetMatrixCell();
			if (MatrixCell)
			{
				return MatrixCell == Element.GetObject();
			}
		}

		return false;
	};

	return ElementWidgets.ContainsByPredicate(IsFaderInUseLambda);
}

void SDMXControlConsoleEditorFaderGroupView::UpdateExpansionState()
{
	if (FaderGroup.IsValid())
	{
		TSharedPtr<SDMXControlConsoleEditorExpandArrowButton> ExpandArrowButton = GetExpandArrowButton();
		if (ExpandArrowButton.IsValid())
		{
			// Get expansion state from model
			const bool bIsExpanded = FaderGroup->IsExpanded();
			ExpandArrowButton->SetExpandArrow(FaderGroup->IsExpanded());
		}
	}
}

void SDMXControlConsoleEditorFaderGroupView::OnExpandArrowClicked(bool bExpand)
{
	if (FaderGroup.IsValid())
	{
		ViewMode = bExpand ? EDMXControlConsoleEditorViewMode::Expanded : EDMXControlConsoleEditorViewMode::Collapsed;

		constexpr bool bNotifyExpansionStateChange = false;
		FaderGroup->Modify();
		FaderGroup->SetIsExpanded(bExpand, bNotifyExpansionStateChange);
	}
}

void SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroup() const
{
	if (!FaderGroup.IsValid())
	{
		return;
	}

	const FScopedTransaction FaderGroupClickedTransaction(LOCTEXT("FaderGroupClickedTransaction", "Add Fader Group"));
	
	UDMXControlConsoleFaderGroupRow& FaderGroupRow = FaderGroup->GetOwnerFaderGroupRowChecked();
	FaderGroupRow.PreEditChange(nullptr);
	UDMXControlConsoleFaderGroup* NewFaderGroup = FaderGroupRow.AddFaderGroup(GetIndex() + 1);
	FaderGroupRow.PostEditChange();

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!ControlConsoleLayouts || !NewFaderGroup)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = ControlConsoleLayouts->GetActiveLayout();
	if (!CurrentLayout)
	{
		return;
	}

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = CurrentLayout->GetLayoutRow(FaderGroup.Get());
	if (!LayoutRow)
	{
		return;
	}

	const int32 Index = LayoutRow->GetIndex(FaderGroup.Get());
	LayoutRow->PreEditChange(nullptr);
	LayoutRow->AddToLayoutRow(NewFaderGroup, Index + 1);
	LayoutRow->PostEditChange();
}

void SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupRow() const
{
	if (!FaderGroup.IsValid())
	{
		return;
	}

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const UDMXControlConsoleEditorLayouts* ControlConsoleLayouts = EditorConsoleModel->GetEditorConsoleLayouts();
	if (!ControlConsoleLayouts)
	{
		return;
	}

	// Add Fader Group next if vertical sorting
	UDMXControlConsoleEditorGlobalLayoutBase* CurrentLayout = ControlConsoleLayouts->GetActiveLayout();
	if (CurrentLayout->GetLayoutMode() == EDMXControlConsoleLayoutMode::Vertical)
	{
		OnAddFaderGroup();
		return;
	}

	UDMXControlConsoleFaderGroupRow& FaderGroupRow = FaderGroup->GetOwnerFaderGroupRowChecked();
	UDMXControlConsoleData& ControlConsoleData = FaderGroupRow.GetOwnerControlConsoleDataChecked();

	const FScopedTransaction FaderGroupRowClickedTransaction(LOCTEXT("FaderGroupRowClickedTransaction", "Add Fader Group"));
	const int32 RowIndex = FaderGroupRow.GetRowIndex();

	ControlConsoleData.PreEditChange(nullptr);
	const UDMXControlConsoleFaderGroupRow* NewRow = ControlConsoleData.AddFaderGroupRow(RowIndex + 1);
	ControlConsoleData.PostEditChange();

	UDMXControlConsoleEditorGlobalLayoutRow* LayoutRow = CurrentLayout->GetLayoutRow(FaderGroup.Get());
	if (!LayoutRow)
	{
		return;
	}

	const int32 LayoutRowIndex = LayoutRow->GetRowIndex();
	CurrentLayout->PreEditChange(nullptr);
	UDMXControlConsoleEditorGlobalLayoutRow* NewLayoutRow = CurrentLayout->AddNewRowToLayout(LayoutRowIndex + 1);
	CurrentLayout->PostEditChange();
	if (!NewLayoutRow)
	{
		return;
	}

	UDMXControlConsoleFaderGroup* NewFaderGroup = NewRow && !NewRow->GetFaderGroups().IsEmpty() ? NewRow->GetFaderGroups()[0] : nullptr;
	NewLayoutRow->PreEditChange(nullptr);
	NewLayoutRow->AddToLayoutRow(NewFaderGroup);
	NewLayoutRow->PostEditChange();
}

void SDMXControlConsoleEditorFaderGroupView::OnFaderGroupFixturePatchChanged(UDMXControlConsoleFaderGroup* InFaderGroup, UDMXEntityFixturePatch* FixturePatch)
{
	if (FaderGroup.IsValid() && FaderGroup == InFaderGroup)
	{
		OnElementAdded();
		OnElementRemoved();
	}
}

FReply SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupClicked() const
{
	OnAddFaderGroup();
	return FReply::Handled();
}

FReply SDMXControlConsoleEditorFaderGroupView::OnAddFaderGroupRowClicked() const
{
	OnAddFaderGroupRow();
	return FReply::Handled();
}

FReply SDMXControlConsoleEditorFaderGroupView::OnAddFaderClicked()
{
	if (FaderGroup.IsValid())
	{
		const FScopedTransaction FaderClickedTransaction(LOCTEXT("FaderClickedTransaction", "Add Fader"));
		FaderGroup->PreEditChange(nullptr);
		FaderGroup->AddRawFader();
		FaderGroup->PostEditChange();
	}

	return FReply::Handled();
}

void SDMXControlConsoleEditorFaderGroupView::OnViewModeChanged()
{
	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	ViewMode = EditorConsoleModel->GetFaderGroupsViewMode();

	if (!FaderGroup.IsValid())
	{
		return;
	}

	switch (ViewMode)
	{
	case EDMXControlConsoleEditorViewMode::Collapsed:
	{
		FaderGroup->SetIsExpanded(false);
		break;
	}
	case EDMXControlConsoleEditorViewMode::Expanded:
		FaderGroup->SetIsExpanded(true);
		break;
	}
}

bool SDMXControlConsoleEditorFaderGroupView::IsCurrentViewMode(EDMXControlConsoleEditorViewMode InViewMode) const
{
	return ViewMode == InViewMode;
}

FOptionalSize SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewHeightByFadersViewMode() const
{
	using namespace UE::DMXControlConsoleEditor::DMXControlConsoleEditorFaderGroupView::Private;

	const UDMXControlConsoleEditorModel* EditorConsoleModel = GetDefault<UDMXControlConsoleEditorModel>();
	const EDMXControlConsoleEditorViewMode FadersViewMode = EditorConsoleModel->GetFadersViewMode();
	return FadersViewMode == EDMXControlConsoleEditorViewMode::Collapsed ? CollapsedViewModeHeight : ExpandedViewModeHeight;
}

FSlateColor SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderColor() const
{
	if (!FaderGroup.IsValid())
	{
		return FLinearColor::White;
	}

	return FaderGroup->GetEditorColor();
}

const FSlateBrush* SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBorderImage() const
{
	if (IsHovered())
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.WhiteBrush");
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.WhiteBrush_Tansparent");
		}
	}
	else
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.WhiteBrush");
		}
		else
		{
			return FAppStyle::GetBrush("NoBorder");
		}
	}
}

const FSlateBrush* SDMXControlConsoleEditorFaderGroupView::GetFaderGroupViewBackgroundBorderImage() const
{
	if (IsHovered())
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroup_Highlighted");
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.DefaultBrush");
		}
	}
	else
	{
		if (IsSelected())
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.FaderGroup_Selected");
		}
		else
		{
			return FDMXControlConsoleEditorStyle::Get().GetBrush("DMXControlConsole.Rounded.DefaultBrush");
		}
	}
}

EVisibility SDMXControlConsoleEditorFaderGroupView::GetViewModeVisibility(EDMXControlConsoleEditorViewMode InViewMode) const
{
	return  IsCurrentViewMode(InViewMode) ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDMXControlConsoleEditorFaderGroupView::GetElementWidgetVisibility(const TScriptInterface<IDMXControlConsoleFaderGroupElement> Element) const
{
	const bool bIsVisible = Element.GetInterface() && Element.GetInterface()->IsMatchingFilter();
	return  bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDMXControlConsoleEditorFaderGroupView::GetAddButtonVisibility() const
{
	const bool bIsVisible =
		FaderGroup.IsValid() &&
		CanAddFaderGroup() &&
		IsCurrentViewMode(EDMXControlConsoleEditorViewMode::Collapsed);

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDMXControlConsoleEditorFaderGroupView::GetAddRowButtonVisibility() const
{
	if (IsCurrentViewMode(EDMXControlConsoleEditorViewMode::Expanded))
	{
		return EVisibility::Collapsed;
	}

	return CanAddFaderGroupRow() ? EVisibility::Visible : EVisibility::Hidden;
}

EVisibility SDMXControlConsoleEditorFaderGroupView::GetElementsHorizontalBoxVisibility() const
{
	const bool bIsVisible =
		GetExpandArrowButton().IsValid() &&
		GetExpandArrowButton()->IsExpanded() &&
		IsCurrentViewMode(EDMXControlConsoleEditorViewMode::Expanded);

	return bIsVisible ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility SDMXControlConsoleEditorFaderGroupView::GetAddFaderButtonVisibility() const
{
	return CanAddFader() ? EVisibility::Visible : EVisibility::Collapsed;
}

#undef LOCTEXT_NAMESPACE
