// Copyright Epic Games, Inc. All Rights Reserved.

#include "Widgets/TableRows/SPropertyAnimatorCoreEditorPropertiesViewTableRow.h"

#include "Framework/Application/SlateApplication.h"
#include "Menus/PropertyAnimatorCoreEditorMenu.h"
#include "Menus/PropertyAnimatorCoreEditorMenuContext.h"
#include "Properties/PropertyAnimatorCoreResolver.h"
#include "Styles/PropertyAnimatorCoreEditorStyle.h"
#include "Subsystems/PropertyAnimatorCoreEditorSubsystem.h"
#include "Subsystems/PropertyAnimatorCoreSubsystem.h"
#include "ToolMenu.h"
#include "ToolMenus.h"
#include "Widgets/DragDropOps/PropertyAnimatorCoreEditorViewDragDropOp.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/SPropertyAnimatorCoreEditorEditPanel.h"
#include "Widgets/TableRows/SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow.h"
#include "Widgets/Views/STileView.h"

#define LOCTEXT_NAMESPACE "PropertyAnimatorCoreEditorPropertiesViewTableRow"

void SPropertyAnimatorCoreEditorPropertiesViewTableRow::Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView, TSharedPtr<SPropertyAnimatorCoreEditorPropertiesView> InView, FPropertiesViewItemPtr InItem)
{
	ViewWeak = InView;
	RowItemWeak = InItem;

	check(InItem.IsValid() && InView.IsValid())

	SMultiColumnTableRow<FPropertiesViewItemPtr>::Construct(
		FSuperRowType::FArguments()
		.Style(&FAppStyle::Get().GetWidgetStyle<FTableRowStyle>("SceneOutliner.TableViewRow"))
		.Padding(5.0f)
		.ShowSelection(true)
		.OnPaintDropIndicator(this, &SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnPropertyPaintDropIndicator)
		.OnDrop(this, &SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnPropertyDrop)
		.OnCanAcceptDrop(this, &SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnPropertyCanAcceptDrop),
		InOwnerTableView
	);

	SetBorderImage(TAttribute<const FSlateBrush*>(this, &SPropertyAnimatorCoreEditorPropertiesViewTableRow::GetBorder));
}

TSharedRef<SWidget> SPropertyAnimatorCoreEditorPropertiesViewTableRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	const FPropertiesViewItemPtr Item = RowItemWeak.Pin();

	if (!Item.IsValid())
	{
		return SNullWidget::NullWidget;
	}

	TSharedRef<SBox> BoxWrapper = SNew(SBox)
		.Padding(FMargin(4.0f, 0.0f))
		.VAlign(VAlign_Center);

	const bool bIsLeafRow = Item->Children.IsEmpty();

	if (InColumnName == SPropertyAnimatorCoreEditorEditPanel::HeaderPropertyColumnName)
	{
		FString PropertyName = Item->Property.GetLeafPropertyName().ToString();

		if (!Item->ParentWeak.IsValid())
		{
			if (const UPropertyAnimatorCoreResolver* Resolver = Item->Property.GetPropertyResolver())
			{
				PropertyName = Resolver->GetResolverName().ToString() + TEXT(".") + PropertyName;
			}
		}

		const FText PropertyDisplayText = FText::FromString(PropertyName);

		BoxWrapper->SetContent(
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SAssignNew(ExpanderArrowWidget, SExpanderArrow, SharedThis(this))
				.Visibility(!bIsLeafRow ? EVisibility::Visible : EVisibility::Hidden)
			]
			+ SHorizontalBox::Slot()
			.FillWidth(1.f)
			.Padding(0.0f, 3.0f, 6.0f, 3.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(PropertyDisplayText)
				.ToolTipText(PropertyDisplayText)
			]
		);
	}
	else if (InColumnName == SPropertyAnimatorCoreEditorEditPanel::HeaderAnimatorColumnName)
	{
		constexpr float ItemSize = 24.f;

		BoxWrapper->SetContent(
			SAssignNew(ControllersTile, STileView<FPropertiesViewControllerItemPtr>)
			.Orientation(Orient_Horizontal)
			.ItemHeight(ItemSize)
			.ItemWidth(ItemSize)
			.SelectionMode(ESelectionMode::Multi)
			.ListItemsSource(&ControllersTileSource)
			.ClearSelectionOnClick(true)
			.OnSelectionChanged(this, &SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnSelectionChanged)
			.OnGenerateTile(this, &SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnGenerateTile)
		);

		for (const TWeakObjectPtr<UPropertyAnimatorCoreBase>& ControllerWeak : Item->ControllersWeak)
		{
			const UPropertyAnimatorCoreBase* Controller = ControllerWeak.Get();

			if (!Controller || !Controller->IsPropertyLinked(Item->Property))
			{
				continue;
			}

			FPropertiesViewControllerItemPtr ControllerItem = MakeShared<FPropertiesViewControllerItem>();
			ControllerItem->Property = MakeShared<FPropertyAnimatorCoreData>(Item->Property);
			ControllerItem->ControllerWeak = ControllerWeak;
			ControllersTileSource.Add(ControllerItem);
		}

		ControllersTile->RebuildList();
	}
	else if (InColumnName == SPropertyAnimatorCoreEditorEditPanel::HeaderActionColumnName)
	{
		const UPropertyAnimatorCoreSubsystem* ControllerSubsystem = UPropertyAnimatorCoreSubsystem::Get();

		// Only show menu if any controller supports this specific property
		if (ControllerSubsystem->IsPropertySupported(Item->Property, false))
		{
			BoxWrapper->SetContent(
				SNew(SComboButton)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Fill)
				.MenuPlacement(EMenuPlacement::MenuPlacement_BelowAnchor)
				.HasDownArrow(false)
				.ButtonStyle(FAppStyle::Get(), "SimpleButton")
				.OnGetMenuContent(this, &SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnGetMenuContent)
				.ButtonContent()
				[
					SNew(SImage)
					.Image(FPropertyAnimatorCoreEditorStyle::Get().GetBrush("PropertyControlIcon.Add"))
					.Visibility(EVisibility::SelfHitTestInvisible)
				]
			);
		}
	}

	return BoxWrapper;
}

TOptional<EItemDropZone> SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnPropertyCanAcceptDrop(const FDragDropEvent& InDragDropEvent, EItemDropZone InDropZone, FPropertiesViewItemPtr InItem)
{
	// Get dragged controllers and check that the property we drop them off is supported by one of them
	if (const TSharedPtr<FPropertyAnimatorCoreEditorViewDragDropOp> DragDropOp = InDragDropEvent.GetOperationAs<FPropertyAnimatorCoreEditorViewDragDropOp>())
	{
		if (InItem.IsValid() && InItem->Property.IsResolved())
		{
			for (const FPropertiesViewControllerItem& DraggedItem : DragDropOp->GetDraggedItems())
			{
				if (const UPropertyAnimatorCoreBase* Controller = DraggedItem.ControllerWeak.Get())
				{
					if (Controller->IsPropertySupported(InItem->Property)
						&& !Controller->IsPropertyLinked(InItem->Property))
					{
						return EItemDropZone::OntoItem;
					}
				}
			}
		}
	}

	return TOptional<EItemDropZone>();
}

int32 SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnPropertyPaintDropIndicator(EItemDropZone InDropZone, const FPaintArgs& InPaintArgs, const FGeometry& InGeometry, const FSlateRect& InSlateRect, FSlateWindowElementList& OutElements, int32 InLayerIndex, const FWidgetStyle& InWidgetStyle, bool InbParentEnabled)
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

FReply SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnPropertyDrop(FDragDropEvent const& InDragDropEvent)
{
	if (const TSharedPtr<FPropertyAnimatorCoreEditorViewDragDropOp> DragDropOp = InDragDropEvent.GetOperationAs<FPropertyAnimatorCoreEditorViewDragDropOp>())
	{
		if (const FPropertiesViewItemPtr RowItem = RowItemWeak.Pin())
		{
			const bool bIsControlPressed = FSlateApplication::Get().GetModifierKeys().IsControlDown();

			for (const FPropertiesViewControllerItem& DraggedItem : DragDropOp->GetDraggedItems())
			{
				if (UPropertyAnimatorCoreBase* Controller = DraggedItem.ControllerWeak.Get())
				{
					if (Controller->IsPropertySupported(RowItem->Property))
					{
						if (!bIsControlPressed && DraggedItem.Property.IsValid())
						{
							Controller->UnlinkProperty(*DraggedItem.Property);
						}

						Controller->LinkProperty(RowItem->Property);
					}
				}
			}
		}

		return FReply::Handled().EndDragDrop();
	}

	return FReply::Unhandled();
}

void SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnSelectionChanged(FPropertiesViewControllerItemPtr InItem, ESelectInfo::Type InSelectInfo)
{
	const TSharedPtr<SPropertyAnimatorCoreEditorPropertiesView> PropertiesView = ViewWeak.Pin();

	if (!PropertiesView.IsValid() || InSelectInfo == ESelectInfo::Direct)
	{
		 return;
	}

	const TSharedPtr<SPropertyAnimatorCoreEditorEditPanel> EditPanel = PropertiesView->GetEditPanel();

	if (!EditPanel.IsValid())
	{
		return;
	}

	TSet<FPropertiesViewControllerItem>& GlobalSelection = EditPanel->GetGlobalSelection();

	if (!FSlateApplication::Get().GetModifierKeys().IsControlDown())
	{
		GlobalSelection.Reset();
	}

	for (FPropertiesViewControllerItemPtr SelectedItem : ControllersTile->GetSelectedItems())
	{
		GlobalSelection.Add(*SelectedItem);
	}

	EditPanel->OnGlobalSelectionChangedDelegate.Broadcast();
}

TSharedRef<ITableRow> SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnGenerateTile(FPropertiesViewControllerItemPtr InItem, const TSharedRef<STableViewBase>& InOwnerTable)
{
	return SNew(SPropertyAnimatorCoreEditorPropertiesViewControllerTableRow, InOwnerTable, SharedThis(this), InItem);
}

TSharedRef<SWidget> SPropertyAnimatorCoreEditorPropertiesViewTableRow::OnGetMenuContent()
{
	static const FName AddControllersMenuName = TEXT("NewAnimatorPropertyViewMenu");

	UToolMenus* ToolMenus = UToolMenus::Get();

	check(ToolMenus);

	if (!ToolMenus->IsMenuRegistered(AddControllersMenuName))
	{
		UToolMenu* AddControllersMenu = ToolMenus->RegisterMenu(AddControllersMenuName);
		AddControllersMenu->AddDynamicSection(TEXT("FillNewAnimatorPropertyViewSection"), FNewToolMenuDelegate::CreateSP(this, &SPropertyAnimatorCoreEditorPropertiesViewTableRow::FillNewAnimatorPropertyViewSection));
	}

	UPropertyAnimatorCoreEditorMenuContext* MenuContextObject = NewObject<UPropertyAnimatorCoreEditorMenuContext>();

	if (const FPropertiesViewItemPtr RowItem = RowItemWeak.Pin())
	{
		MenuContextObject->SetPropertyData(RowItem->Property);
	}

	const FToolMenuContext MenuContext(MenuContextObject);
	return ToolMenus->GenerateWidget(AddControllersMenuName, MenuContext);
}

void SPropertyAnimatorCoreEditorPropertiesViewTableRow::FillNewAnimatorPropertyViewSection(UToolMenu* InToolMenu) const
{
	if (!InToolMenu)
	{
		return;
	}

	const UPropertyAnimatorCoreEditorMenuContext* Context = InToolMenu->FindContext<UPropertyAnimatorCoreEditorMenuContext>();

	if (!Context)
	{
		return;
	}

	UPropertyAnimatorCoreEditorSubsystem* EditorSubsystem = UPropertyAnimatorCoreEditorSubsystem::Get();

	if (!EditorSubsystem)
	{
		return;
	}

	const FPropertyAnimatorCoreEditorMenuContext MenuContext({}, {Context->GetPropertyData()});
	const FPropertyAnimatorCoreEditorMenuOptions MenuOptions({EPropertyAnimatorCoreEditorMenuType::New});
	EditorSubsystem->FillAnimatorMenu(InToolMenu, MenuContext, MenuOptions);
}

#undef LOCTEXT_NAMESPACE
