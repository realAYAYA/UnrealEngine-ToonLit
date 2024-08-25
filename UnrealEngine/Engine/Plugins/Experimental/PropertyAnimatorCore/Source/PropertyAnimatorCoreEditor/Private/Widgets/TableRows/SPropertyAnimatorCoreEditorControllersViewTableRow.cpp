// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/TableRows/SPropertyAnimatorCoreEditorControllersViewTableRow.h"

#include "Framework/Application/SlateApplication.h"
#include "Styling/SlateIconFinder.h"
#include "Widgets/SPropertyAnimatorCoreEditorEditPanel.h"
#include "Widgets/DragDropOps/PropertyAnimatorCoreEditorViewDragDropOp.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Views/SPropertyAnimatorCoreEditorControllersView.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreEditorControllerTableRow"

SPropertyAnimatorCoreEditorControllersViewTableRow::~SPropertyAnimatorCoreEditorControllersViewTableRow()
{
	if (const TSharedPtr<SPropertyAnimatorCoreEditorControllersView> View = ViewWeak.Pin())
	{
		if (const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = View->GetEditPanel())
		{
			EditPanel->OnGlobalSelectionChangedDelegate.RemoveAll(this);
		}
	}
}

void SPropertyAnimatorCoreEditorControllersViewTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<SPropertyAnimatorCoreEditorControllersView> InView, FControllersViewItemPtr InItem)
{
	ViewWeak = InView;
	RowItemWeak = InItem;

	check(InItem.IsValid())

	const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = InView->GetEditPanel();

	check(EditPanel.IsValid())

	EditPanel->OnGlobalSelectionChangedDelegate.AddSP(this, &SPropertyAnimatorCoreEditorControllersViewTableRow::OnGlobalSelectionChanged);
	EditPanel->OnControllerRenameRequestedDelegate.AddSP(this, &SPropertyAnimatorCoreEditorControllersViewTableRow::OnControllerRenameRequested);

	SMultiColumnTableRow<FControllersViewItemPtr>::Construct(
		FSuperRowType::FArguments()
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"))
		.Padding(0.f)
		.ShowSelection(true)
		.OnDragDetected(EditPanel.Get(), &SPropertyAnimatorCoreEditorEditPanel::OnDragStart)
		.OnPaintDropIndicator(this, &SPropertyAnimatorCoreEditorControllersViewTableRow::OnPropertyPaintDropIndicator)
		.OnDrop(this, &SPropertyAnimatorCoreEditorControllersViewTableRow::OnPropertyDrop)
		.OnCanAcceptDrop(this, &SPropertyAnimatorCoreEditorControllersViewTableRow::OnPropertyCanAcceptDrop),
		InOwnerTableView
	);

	SetBorderImage(TAttribute<const FSlateBrush*>(this, &SPropertyAnimatorCoreEditorControllersViewTableRow::GetBorder));
}

TSharedRef<SWidget> SPropertyAnimatorCoreEditorControllersViewTableRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	const FControllersViewItemPtr Item = RowItemWeak.Pin();

	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	const UPropertyAnimatorCoreBase* Controller = Item->ControlledProperty.ControllerWeak.Get();

	if (!Controller)
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SBox> BoxWrapper = SNew(SBox)
		.Visibility(EVisibility::Visible)
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Fill);

	if (InColumnName == SPropertyAnimatorCoreEditorEditPanel::HeaderAnimatorColumnName)
	{
		const FText ControllerDisplayText = FText::FromString(Controller->GetAnimatorDisplayName());
		const FText ControllerNameText = FText::FromName(Controller->GetAnimatorOriginalName());
		const FSlateIcon ControllerIcon = FSlateIconFinder::FindIconForClass(Controller->GetClass());

		BoxWrapper->SetContent(
			SNew(SHorizontalBox)
			.Visibility(!Item->ControlledProperty.Property.IsValid() ? EVisibility::Visible : EVisibility::Hidden)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(ExpanderArrowWidget, SExpanderArrow, SharedThis(this))
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Center)
			.AutoWidth()
			.Padding(5.f, 0.f)
			[
				SNew(SScaleBox)
				.Stretch(EStretch::ScaleToFit)
				[
					SNew(SImage)
					.Image(ControllerIcon.GetIcon())
				]
			]
			+ SHorizontalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.FillWidth(1.f)
			.Padding(5.f, 0.f)
			[
				SNew(SVerticalBox)
				.Visibility(EVisibility::Visible)
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.Padding(0.f, 5.f)
				.AutoHeight()
				[
					SAssignNew(ControllerEditableTextBox, SEditableTextBox)
					.BackgroundColor(FSlateColor(FLinearColor::Transparent))
					.Visibility(EVisibility::HitTestInvisible)
					.Padding(0.f)
					.Justification(ETextJustify::Left)
					.Text(ControllerDisplayText)
					.ToolTipText(ControllerDisplayText)
					.IsReadOnly(true)
					.RevertTextOnEscape(true)
					.SelectAllTextWhenFocused(true)
					.OnVerifyTextChanged(this, &SPropertyAnimatorCoreEditorControllersViewTableRow::OnVerifyControllerTextChanged)
					.OnTextCommitted(this, &SPropertyAnimatorCoreEditorControllersViewTableRow::OnControllerTextCommitted)
				]
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Fill)
				.Padding(0.f, 5.f)
				.AutoHeight()
				[
					SNew(STextBlock)
					.Margin(0.f)
					.Justification(ETextJustify::Left)
					.Text(ControllerNameText)
				]
			]
		);

		// Handle renaming when double clicking
		BoxWrapper->SetOnMouseDoubleClick(FPointerEventHandler::CreateSP(this, &SPropertyAnimatorCoreEditorControllersViewTableRow::OnControllerBoxDoubleClick));

		// Remove border around editable box to make it look like a standard text box
		ControllerEditableTextBox->SetBorderBackgroundColor(FSlateColor(FLinearColor::Transparent));
		ControllerEditableTextBox->SetBorderImage(nullptr);
	}
	else if (InColumnName == SPropertyAnimatorCoreEditorEditPanel::HeaderPropertyColumnName)
	{
		const FText PropertyDisplayName = Item->ControlledProperty.Property.IsValid()
			? FText::FromName(Item->ControlledProperty.Property->GetPropertyDisplayName())
			: FText::Format(LOCTEXT("AnimatorPropertiesLinkedCount", "{0} Propertie(s) Linked"), Controller->GetLinkedPropertiesCount());

		BoxWrapper->SetContent(
			SNew(STextBlock)
			.Text(PropertyDisplayName)
			.ToolTipText(PropertyDisplayName)
		);

		BoxWrapper->SetVAlign(VAlign_Center);
	}

	return BoxWrapper;
}

int32 SPropertyAnimatorCoreEditorControllersViewTableRow::OnPropertyPaintDropIndicator(EItemDropZone InDropZone, const FPaintArgs& InPaintArgs, const FGeometry& InGeometry, const FSlateRect& InSlateRect, FSlateWindowElementList& OutElements, int32 InLayerIndex, const FWidgetStyle& InWidgetStyle, bool InbParentEnabled)
{
	const FSlateBrush* DropIndicatorBrush = GetDropIndicatorBrush(InDropZone);

	constexpr float OffsetX = 10.0f;
	const FVector2D Offset(OffsetX * GetIndentLevel(), 0.f);
	FSlateDrawElement::MakeBox
	(
		OutElements,
		InLayerIndex++,
		InGeometry.ToPaintGeometry(FVector2D(InGeometry.GetLocalSize() - Offset), FSlateLayoutTransform(Offset)),
		DropIndicatorBrush,
		ESlateDrawEffect::None,
		DropIndicatorBrush->GetTint(InWidgetStyle) * InWidgetStyle.GetColorAndOpacityTint()
	);

	return InLayerIndex;
}

FReply SPropertyAnimatorCoreEditorControllersViewTableRow::OnPropertyDrop(FDragDropEvent const& InDragDropEvent)
{
	if (const TSharedPtr<FPropertyAnimatorCoreEditorViewDragDropOp> DragDropOp = InDragDropEvent.GetOperationAs<FPropertyAnimatorCoreEditorViewDragDropOp>())
	{
		if (const FControllersViewItemPtr RowItem = RowItemWeak.Pin())
		{
			if (UPropertyAnimatorCoreBase* Controller = RowItem->ControlledProperty.ControllerWeak.Get())
			{
				const bool bIsControlPressed = FSlateApplication::Get().GetModifierKeys().IsControlDown();

				for (const FPropertiesViewControllerItem& DraggedItem : DragDropOp->GetDraggedItems())
				{
					if (!DraggedItem.Property.IsValid())
					{
						continue;
					}

					// Unlink from current controller and link to new
					UPropertyAnimatorCoreBase* PreviousController = DraggedItem.ControllerWeak.Get();
					if (!bIsControlPressed && PreviousController)
				    {
				    	PreviousController->UnlinkProperty(*DraggedItem.Property);
				    }

					Controller->LinkProperty(*DraggedItem.Property);
				}
			}
		}

		return FReply::Handled().EndDragDrop();
	}

	return FReply::Unhandled();
}

TOptional<EItemDropZone> SPropertyAnimatorCoreEditorControllersViewTableRow::OnPropertyCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, FControllersViewItemPtr InItem)
{
	// Get dragged properties and check that the controller we drop them on supports them
	if (const TSharedPtr<FPropertyAnimatorCoreEditorViewDragDropOp> DragDropOp = InDragDropEvent.GetOperationAs<FPropertyAnimatorCoreEditorViewDragDropOp>())
	{
		const UPropertyAnimatorCoreBase* Controller = InItem->ControlledProperty.ControllerWeak.Get();

		if (InItem.IsValid()
			&& !InItem->ControlledProperty.Property.IsValid()
			&& Controller)
		{
			for (const FPropertiesViewControllerItem& DraggedItem : DragDropOp->GetDraggedItems())
			{
				if (const FPropertyAnimatorCoreData* Property = DraggedItem.Property.Get())
				{
					if (Controller->IsPropertySupported(*Property)
						&& !Controller->IsPropertyLinked(*Property))
					{
						return EItemDropZone::OntoItem;
					}
				}
			}
		}
	}

	return TOptional<EItemDropZone>();
}

void SPropertyAnimatorCoreEditorControllersViewTableRow::OnGlobalSelectionChanged()
{
	const TSharedPtr<SPropertyAnimatorCoreEditorControllersView> ControllersView = ViewWeak.Pin();

	if (!ControllersView.IsValid())
	{
		return;
	}

	const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = ControllersView->GetEditPanel();

	if (!EditPanel.IsValid())
	{
		return;
	}

	const FControllersViewItemPtr RowItem = RowItemWeak.Pin();

	if (!RowItem.IsValid())
	{
		return;
	}

	const TSet<FPropertiesViewControllerItem>& GlobalSelection = EditPanel->GetGlobalSelection();

	if (GlobalSelection.Contains(RowItem->ControlledProperty))
	{
		ControllersView->ControllersTree->SetItemSelection(RowItem, true, ESelectInfo::Direct);
	}
	else
	{
		ControllersView->ControllersTree->SetItemSelection(RowItem, false, ESelectInfo::Direct);
	}
}

void SPropertyAnimatorCoreEditorControllersViewTableRow::OnControllerRenameRequested(UPropertyAnimatorCoreBase* InController)
{
	const FControllersViewItemPtr ItemRow = RowItemWeak.Pin();
	if (!ItemRow.IsValid())
	{
		return;
	}

	if (InController != ItemRow->ControlledProperty.ControllerWeak.Get())
	{
		return;
	}

	BeginRenamingOperation();
}

bool SPropertyAnimatorCoreEditorControllersViewTableRow::OnVerifyControllerTextChanged(const FText& InText, FText& OutError) const
{
	if (!ControllerEditableTextBox.IsValid() || ControllerEditableTextBox->IsReadOnly() || !RowItemWeak.IsValid())
	{
		OutError = LOCTEXT("ControllerTextInvalidRow", "Invalid row item");
		return false;
	}

	if (InText.IsEmpty())
	{
		OutError = LOCTEXT("ControllerTextInvalidName", "Invalid animator name");
		return false;
	}

	// Add more rules here

	return true;
}

void SPropertyAnimatorCoreEditorControllersViewTableRow::OnControllerTextCommitted(const FText& InText, ETextCommit::Type InType) const
{
	if (!ControllerEditableTextBox.IsValid() || ControllerEditableTextBox->IsReadOnly())
	{
		return;
	}

	const FControllersViewItemPtr ItemRow = RowItemWeak.Pin();
	if (!ItemRow.IsValid())
	{
		return;
	}

	UPropertyAnimatorCoreBase* Controller = ItemRow->ControlledProperty.ControllerWeak.Get();
	if (!Controller || Controller->IsTemplate())
	{
		return;
	}

	EndRenamingOperation();

	if (InType == ETextCommit::OnEnter)
	{
		const FName NewControllerDisplayName(InText.ToString());
		Controller->SetAnimatorDisplayName(NewControllerDisplayName);
		ControllerEditableTextBox->SetText(InText);
	}
	else
	{
		const FText OriginalDisplayNameText = FText::FromString(Controller->GetAnimatorDisplayName());
		ControllerEditableTextBox->SetText(OriginalDisplayNameText);
	}
}

FReply SPropertyAnimatorCoreEditorControllersViewTableRow::OnControllerBoxDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InEvent) const
{
	BeginRenamingOperation();
	return FReply::Handled();
}

void SPropertyAnimatorCoreEditorControllersViewTableRow::BeginRenamingOperation() const
{
	if (!ControllerEditableTextBox.IsValid() || !ControllerEditableTextBox->IsReadOnly())
	{
		return;
	}

	const FEditableTextBoxStyle& EditableStyle = FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");

	ControllerEditableTextBox->SetBorderBackgroundColor(FLinearColor::White);
	ControllerEditableTextBox->SetBorderImage(&EditableStyle.BackgroundImageFocused);
	ControllerEditableTextBox->SetIsReadOnly(false);
	ControllerEditableTextBox->SetVisibility(EVisibility::Visible);

	FSlateApplication::Get().SetKeyboardFocus(ControllerEditableTextBox);
}

void SPropertyAnimatorCoreEditorControllersViewTableRow::EndRenamingOperation() const
{
	if (!ControllerEditableTextBox.IsValid() || ControllerEditableTextBox->IsReadOnly())
	{
		return;
	}

	ControllerEditableTextBox->SetBorderImage(nullptr);
	ControllerEditableTextBox->SetVisibility(EVisibility::HitTestInvisible);
	ControllerEditableTextBox->SetIsReadOnly(true);
}

#undef LOCTEXT_NAMESPACE