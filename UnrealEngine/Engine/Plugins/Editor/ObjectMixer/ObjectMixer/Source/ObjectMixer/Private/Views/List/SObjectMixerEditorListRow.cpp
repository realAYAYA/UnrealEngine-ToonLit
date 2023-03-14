// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/List/SObjectMixerEditorListRow.h"

#include "EditorClassUtils.h"
#include "ObjectMixerEditorStyle.h"
#include "Views/List/ObjectMixerEditorList.h"
#include "Views/List/SObjectMixerEditorList.h"
#include "Views/Widgets/SHyperlinkWithTextHighlight.h"

#include "Customizations/ColorStructCustomization.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "GameFramework/Actor.h"
#include "Input/DragAndDrop.h"
#include "ISinglePropertyView.h"
#include "Modules/ModuleManager.h"
#include "PropertyEditorModule.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Styling/AppStyle.h"
#include "Styling/StyleColors.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "ObjectMixerEditor"

const FText DropFormatText = LOCTEXT("DropFormatText", "{0} {1} {2} {3}");
const FText MultiDragFormatText = LOCTEXT("MultiDragFormatText", "{0} Items");

void SObjectMixerEditorListRow::Construct(
	const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable,
	const TWeakPtr<FObjectMixerEditorListRow> InRow)
{
	check(InRow.IsValid());

	Item = InRow;
	HybridRowIndex = Item.Pin()->GetOrFindHybridRowIndex();

	SMultiColumnTableRow<FObjectMixerEditorListRowPtr>::Construct(
		FSuperRowType::FArguments()
		.Padding(1.0f)
		.OnCanAcceptDrop(this, &SObjectMixerEditorListRow::HandleCanAcceptDrop)
		.OnAcceptDrop(this, &SObjectMixerEditorListRow::HandleAcceptDrop)
		.OnDragDetected(this, &SObjectMixerEditorListRow::HandleDragDetected)
		.OnDragLeave(this, &SObjectMixerEditorListRow::HandleDragLeave)
	 	, InOwnerTable
	);

	const FName VisibleHoveredBrushName = TEXT("Level.VisibleHighlightIcon16x");
	const FName VisibleNotHoveredBrushName = TEXT("Level.VisibleIcon16x");
	const FName NotVisibleHoveredBrushName = TEXT("Level.NotVisibleHighlightIcon16x");
	const FName NotVisibleNotHoveredBrushName = TEXT("Level.NotVisibleIcon16x");

	VisibleHoveredBrush = FAppStyle::Get().GetBrush(VisibleHoveredBrushName);
	VisibleNotHoveredBrush = FAppStyle::Get().GetBrush(VisibleNotHoveredBrushName);
	NotVisibleHoveredBrush = FAppStyle::Get().GetBrush(NotVisibleHoveredBrushName);
	NotVisibleNotHoveredBrush = FAppStyle::Get().GetBrush(NotVisibleNotHoveredBrushName);
	
	SoloOnBrush = FObjectMixerEditorStyle::Get().GetBrush("ObjectMixer.Solo");
	SoloOffHoveredBrush = FObjectMixerEditorStyle::Get().GetBrush("ObjectMixer.SoloHoverOff");
}

TSharedRef<SWidget> SObjectMixerEditorListRow::GenerateWidgetForColumn(const FName& InColumnName)
{
	check(Item.IsValid());
	const FObjectMixerEditorListRowPtr RowPtr = GetHybridChildOrRowItemIfNull();

	if (const TSharedPtr<SWidget> CellWidget = GenerateCells(InColumnName, RowPtr))
	{
		if (InColumnName == SObjectMixerEditorList::ItemNameColumnName)
		{			
			// The first column gets the tree expansion arrow for this row
			return SNew(SBox)
				.MinDesiredHeight(20)
				[
					SNew( SHorizontalBox )

					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(6, 0, 0, 0)
					[
						SNew( SExpanderArrow, SharedThis(this) ).IndentAmount(12)
					]

					+SHorizontalBox::Slot()
					.FillWidth(1.0f)
					[
						CellWidget.ToSharedRef()
					]
				];
		}
		
		return SNew(SBox)
				   .HAlign(HAlign_Fill)
				   .VAlign(VAlign_Center)
			   [
				   CellWidget.ToSharedRef()
			   ];
	}

	return SNullWidget::NullWidget;
}

void SObjectMixerEditorListRow::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	bIsHovered = true;
	
	SMultiColumnTableRow<FObjectMixerEditorListRowPtr>::OnMouseEnter(MyGeometry, MouseEvent);
}

void SObjectMixerEditorListRow::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	bIsHovered = false;

	SMultiColumnTableRow<FObjectMixerEditorListRowPtr>::OnMouseLeave(MouseEvent);
}

SObjectMixerEditorListRow::~SObjectMixerEditorListRow()
{
	Item.Reset();
}

FReply SObjectMixerEditorListRow::HandleDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	TArray<FObjectMixerEditorListRowPtr> DraggedItems = Item.Pin()->GetSelectedTreeViewItems();
	TSharedRef<FObjectMixerListRowDragDropOp> Operation =
		FObjectMixerListRowDragDropOp::New(DraggedItems);

	return FReply::Handled().BeginDragDrop(Operation);
}

void SObjectMixerEditorListRow::HandleDragLeave(const FDragDropEvent& DragDropEvent)
{
	if (TSharedPtr<FObjectMixerListRowDragDropOp> Operation =
		DragDropEvent.GetOperationAs<FObjectMixerListRowDragDropOp>())
	{
		Operation->ResetToDefaultToolTip();
	}
}

TOptional<EItemDropZone> SObjectMixerEditorListRow::HandleCanAcceptDrop(
	const FDragDropEvent& DragDropEvent, EItemDropZone DropZone, FObjectMixerEditorListRowPtr TargetItem)
{
	TSharedPtr<FObjectMixerListRowDragDropOp> Operation =
		DragDropEvent.GetOperationAs<FObjectMixerListRowDragDropOp>();

	if (!Operation.IsValid())
	{
		return TOptional<EItemDropZone>();
	}

	const UObject* DropOnObject = TargetItem->GetObject();
	const bool bIsDroppingOnFolderRow = TargetItem->GetRowType() == FObjectMixerEditorListRow::Folder;

	const bool bIsDropDenied =
		(!DropOnObject && !bIsDroppingOnFolderRow) ||
		(DropOnObject && DropOnObject->IsA(UActorComponent::StaticClass())) ||
		(
			Operation->DraggedItems.Num() == 1 &&
			Operation->DraggedItems[0]->GetObject() &&
			Operation->DraggedItems[0]->GetObject()->IsA(UActorComponent::StaticClass())
		) ||
		(
			Operation->DraggedItems.Num() == 1 &&
			Operation->DraggedItems[0]->GetRowType() == FObjectMixerEditorListRow::Folder
		)
	;

	if (bIsDropDenied)
	{
		Operation->SetToolTip(
			LOCTEXT("ObjectMixerDragDropWarning", "Drop an actor row onto another actor row or folder to set attach parent or folder.\nDrop any row onto a collection button to assign a collection to the row."),
			FAppStyle::Get().GetBrush("Graph.ConnectorFeedback.Error")
		);

		return TOptional<EItemDropZone>();
	}

	FText ItemNameText = FText::GetEmpty();

	if (Operation->DraggedItems.Num() == 1)
	{
		ItemNameText = Operation->DraggedItems[0]->GetDisplayName();
	}
	else
	{
		ItemNameText = FText::Format(MultiDragFormatText, FText::AsNumber(Operation->DraggedItems.Num()));
	}

	const FText DropPermittedText =
		FText::Format(DropFormatText,
			bIsDroppingOnFolderRow ? LOCTEXT("DragDropMoveToFolderPrefix", "Move") : LOCTEXT("DragDropSetAttachParentPrefix", "Set"),
			ItemNameText,
			bIsDroppingOnFolderRow ? LOCTEXT("DragDropMoveToFolderMidfix", "into") : LOCTEXT("DragDropSetAttachParentMidfix", "AttachParent as"),
			TargetItem->GetDisplayName()
		);

	Operation->SetToolTip(
		DropPermittedText,
		FAppStyle::Get().GetBrush("Graph.ConnectorFeedback.OK")
	);

	// We have no behaviour yet for dropping one item onto another, so we'll treat it like we dropped it above
	return EItemDropZone::OntoItem;
}

FReply SObjectMixerEditorListRow::HandleAcceptDrop(const FDragDropEvent& DragDropEvent, EItemDropZone DropZone,
                                                        FObjectMixerEditorListRowPtr TargetItem)
{
	TSharedPtr<FObjectMixerListRowDragDropOp> Operation =
		DragDropEvent.GetOperationAs<FObjectMixerListRowDragDropOp>();

	if (!Operation.IsValid())
	{
		return FReply::Unhandled();
	}

	UObject* DropOnObject = TargetItem->GetObject();
	const bool bIsDroppingOnFolderRow = TargetItem->GetRowType() == FObjectMixerEditorListRow::Folder && TargetItem->GetFolderPath() != NAME_None;
	FAttachmentTransformRules Rules(EAttachmentRule::KeepWorld, false);

	FScopedTransaction DragDropTransaction(LOCTEXT("ObjectMixerDragDropTransaction","Object Mixer Drag & Drop"));

	for (const FObjectMixerEditorListRowPtr& DraggedItem : Operation->DraggedItems)
	{
		if (AActor* ObjectAsActor = Cast<AActor>(DraggedItem->GetObject()))
		{
			if (bIsDroppingOnFolderRow)
			{
				ObjectAsActor->Modify();
				ObjectAsActor->SetFolderPath(TargetItem->GetFolderPath());
			}
			else if (AActor* DropOnObjectAsActor = Cast<AActor>(DropOnObject))
			{
				ObjectAsActor->Modify();
				ObjectAsActor->AttachToActor(DropOnObjectAsActor, Rules);
			}
		}
	}

	return FReply::Handled();
}

FObjectMixerEditorListRowPtr SObjectMixerEditorListRow::GetHybridChildOrRowItemIfNull() const
{
	if (TSharedPtr<FObjectMixerEditorListRow> PinnedItem = Item.Pin())
	{
		if (FObjectMixerEditorListRowPtr HybridChild = HybridRowIndex != INDEX_NONE ? PinnedItem->GetChildRows()[HybridRowIndex] : nullptr)
		{
			return HybridChild;
		}
			
		return PinnedItem;
	}

	return nullptr;
}

bool SObjectMixerEditorListRow::GetIsItemOrHybridChildSelected() const
{
	if (Item.IsValid())
	{
		const bool bIsItemSelected = Item.Pin()->GetIsSelected();
		const bool bHasHybridChild = HybridRowIndex != INDEX_NONE && Item.Pin()->GetChildRows()[HybridRowIndex].IsValid();
		const bool bIsChildSelected = bHasHybridChild && Item.Pin()->GetChildRows()[HybridRowIndex]->GetIsSelected();
		return bIsChildSelected  || bIsItemSelected;
	}

	return false;
}

bool SObjectMixerEditorListRow::IsVisible() const
{
	if (const TSharedPtr<FObjectMixerEditorListRow> PinnedItem = Item.Pin())
	{
		return PinnedItem->GetObjectVisibility();
	}

	return false;
}

FSlateColor SObjectMixerEditorListRow::GetVisibilityIconForegroundColor() const
{
	check(Item.IsValid());
	
	const bool bIsSelected = Item.Pin()->GetIsSelected();

	// make the foreground brush transparent if it is not selected and it is visible
	if (IsVisible() && !bIsHovered && !bIsSelected)
	{
		return FLinearColor::Transparent;
	}
	else if (bIsHovered && !bIsSelected)
	{
		return FStyleColors::ForegroundHover;
	}

	return FSlateColor::UseForeground();
}

FSlateColor SObjectMixerEditorListRow::GetSoloIconForegroundColor() const
{
	check(Item.IsValid());

	const bool bIsSelected = Item.Pin()->GetIsSelected();

	// make the foreground brush transparent if it is not selected, hovered or solo
	if (!Item.Pin()->IsThisRowSolo() && !bIsHovered && !bIsSelected)
	{
		return FLinearColor::Transparent;
	}
	else if (bIsHovered && !bIsSelected)
	{
		return FStyleColors::ForegroundHover;
	}

	return FSlateColor::UseForeground();
}

const FSlateBrush* SObjectMixerEditorListRow::GetVisibilityBrush() const
{
	if (IsVisible())
	{
		return bIsHovered ? VisibleHoveredBrush : VisibleNotHoveredBrush;
	}
	
	return bIsHovered ? NotVisibleHoveredBrush : NotVisibleNotHoveredBrush;
}

const FSlateBrush* SObjectMixerEditorListRow::GetSoloBrush() const
{
	check(Item.IsValid());

	// make the foreground brush transparent if it is not selected, hovered or solo
	if (Item.Pin()->IsThisRowSolo())
	{
		return SoloOnBrush;
	}

	return SoloOffHoveredBrush;
}

TSharedPtr<SWidget> SObjectMixerEditorListRow::GenerateCells(
	const FName& InColumnName, const TSharedPtr<FObjectMixerEditorListRow> RowPtr)
{
	check(RowPtr.IsValid());
	
	if (RowPtr->GetRowType() == FObjectMixerEditorListRow::None)
	{
		return SNullWidget::NullWidget;
	}
	
	const bool bIsHybridRow = HybridRowIndex != INDEX_NONE;
	
	if (InColumnName.IsEqual(SObjectMixerEditorList::ItemNameColumnName))
	{
		TSharedRef<SHorizontalBox> HBox = SNew(SHorizontalBox);
		
		HBox->AddSlot()
		.AutoWidth()
		[
			SNew(SImage)
			.Image_Lambda([RowPtr]()
			{
				return RowPtr->GetObjectIconBrush();
			})
			.ColorAndOpacity(FSlateColor::UseForeground())
		];

		bool bNeedsStandardTextBlock = true;
		const FText DisplayName = RowPtr->GetDisplayName(bIsHybridRow);
		
		if (TObjectPtr<UObject> Object = RowPtr->GetObject())
		{
			if (UClass* ActorClass = Object->GetClass())
			{
				if (UBlueprint* AsBlueprint = UBlueprint::GetBlueprintFromClass(ActorClass))
				{					
					bNeedsStandardTextBlock = false;
			
					HBox->AddSlot()
					.Padding(FMargin(10.0, 0, 0, 0))
					[
						SNew(SHyperlinkWithTextHighlight)
						.Style(FAppStyle::Get(), "Common.GotoBlueprintHyperlink")
						.Visibility(EVisibility::Visible)
						.Text(DisplayName)
						.ToolTipText(LOCTEXT("ClickToEditBlueprint", "Click to edit Blueprint"))
						.OnNavigate(this, &SObjectMixerEditorListRow::OnClickBlueprintLink, AsBlueprint, Object.Get())
						.HighlightText(this, &SObjectMixerEditorListRow::GetHighlightText)
					];
				}
			}
		}

		if (bNeedsStandardTextBlock)
		{
			FText TooltipText = DisplayName;

			if (const UObjectMixerObjectFilter* Filter = RowPtr->GetObjectFilter())
			{
				if (const TObjectPtr<UObject> Object = RowPtr->GetObject())
				{
					TooltipText = Filter->GetRowTooltipText(Object, bIsHybridRow);
				}
			}
			
			HBox->AddSlot()
			.Padding(FMargin(10.0, 0, 0, 0))
			[
				SNew(STextBlock)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.Justification(ETextJustify::Left)
				.Text(DisplayName)
				.ToolTipText(TooltipText)
				.HighlightText(this, &SObjectMixerEditorListRow::GetHighlightText)
			];
		}
		
		return SNew(SBox)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Padding(FMargin(TextBlockLeftPadding, 0.f, 0.f, 0.f))
				[
					HBox
				];
	}

	if (InColumnName.IsEqual(SObjectMixerEditorList::EditorVisibilityColumnName))
	{
		if (!bIsHybridRow && RowPtr->GetObject() && !RowPtr->GetObject()->IsA(AActor::StaticClass()))
		{
			return nullptr;
		}
		
		return SNew(SBox)
				.HAlign(HAlign_Left)
				.VAlign(VAlign_Center)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.Padding(FMargin(2,0,0,0))
				[
					SNew(SImage)
					.ColorAndOpacity(this, &SObjectMixerEditorListRow::GetVisibilityIconForegroundColor)
					.Image_Raw(this, &SObjectMixerEditorListRow::GetVisibilityBrush)
					.OnMouseButtonDown_Lambda(
						[RowPtr] (const FGeometry& MyGeometry, const FPointerEvent& Event)
						{
							check (RowPtr);

							FScopedTransaction Transaction( LOCTEXT("VisibilityChanged", "Object Mixer - Visibility Changed") );

							const bool bIsVisible = RowPtr->GetObjectVisibility();
							
							if (const TSharedPtr<SObjectMixerEditorList> PinnedListView = RowPtr->GetListViewPtr().Pin();
								RowPtr->GetIsSelected() && PinnedListView->GetSelectedTreeViewItemCount() > 0)
							{
								PinnedListView->SetSelectedTreeViewItemActorsEditorVisible(!bIsVisible);

								return FReply::Handled();
							}

							// Set Visibility Recursively
							RowPtr->SetObjectVisibility(!bIsVisible, true);

							return FReply::Handled();
						}
					)
				]
			;
	}

	if (InColumnName.IsEqual(SObjectMixerEditorList::EditorVisibilitySoloColumnName))
	{
		if (!bIsHybridRow && RowPtr->GetObject() && !RowPtr->GetObject()->IsA(AActor::StaticClass()))
		{
			return nullptr;
		}
		
		return SNew(SBox)
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Visibility(EVisibility::SelfHitTestInvisible)
				.Padding(0.f)
				[
					SNew(SImage)
					.ColorAndOpacity(this, &SObjectMixerEditorListRow::GetSoloIconForegroundColor)
					.Image(this, &SObjectMixerEditorListRow::GetSoloBrush)
					.OnMouseButtonDown_Lambda(
						[this] (const FGeometry& MyGeometry, const FPointerEvent& Event)
						{
							check (Item.IsValid());

							const FObjectMixerEditorListRowPtr PinnedItem = Item.Pin();
							
							if (const TSharedPtr<SObjectMixerEditorList> PinnedListView = PinnedItem->GetListViewPtr().Pin();
								PinnedListView->GetTreeViewItemCount() > 0)
							{
								const bool bIsRowSolo = PinnedItem->IsThisRowSolo();
								for (const TSharedPtr<FObjectMixerEditorListRow>& TreeItem : PinnedListView->GetTreeViewItems())
								{
									TreeItem->SetObjectVisibility(bIsRowSolo, true);
								}

								if (!bIsRowSolo)
								{
									PinnedItem->SetObjectVisibility(true, true);
									PinnedItem->ClearSoloRows();
								}
								
								PinnedItem->SetRowSoloState(!bIsRowSolo);

								return FReply::Handled();
							}

							return FReply::Unhandled();
						}
					)
				]
			;
	}
	
	if (UObject* ObjectRef = RowPtr->GetObject())
	{
		FPropertyEditorModule& PropertyEditorModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");
		{
			FSinglePropertyParams Params;
			Params.NamePlacement = EPropertyNamePlacement::Hidden;
			
			const TSharedPtr<ISinglePropertyView> SinglePropertyView =
				PropertyEditorModule.CreateSingleProperty(ObjectRef, InColumnName, Params
			);

			if (SinglePropertyView)
			{
				if (const TSharedPtr<IPropertyHandle> Handle = SinglePropertyView->GetPropertyHandle())
				{
					if (const FProperty* Property = Handle->GetProperty())
					{
						const FName PropertyName = Property->GetFName();
						RowPtr->PropertyNamesToHandles.Add(PropertyName, Handle);
						
						// Simultaneously edit all selected rows with a similar property
						FSimpleDelegate OnPropertyValueChanged =
							FSimpleDelegate::CreateRaw(
								this,
								&SObjectMixerEditorListRow::OnPropertyChanged, PropertyName);
					
						Handle->SetOnPropertyValueChanged(OnPropertyValueChanged);
						Handle->SetOnChildPropertyValueChanged(OnPropertyValueChanged);

						return SNew(SBox)
								.Visibility(EVisibility::SelfHitTestInvisible)
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Center)
								[
									SinglePropertyView.ToSharedRef()
								];
					}
				}
			}
		}
	}

	return nullptr;
}

void SObjectMixerEditorListRow::OnPropertyChanged(const FName PropertyName) const
{
	check(Item.IsValid());

	struct Local
	{
		static void SetValueOnSelectedItems(
			const FString& ValueAsString, const TArray<FObjectMixerEditorListRowPtr>& OtherSelectedItems,
			const FName& PropertyName, const FObjectMixerEditorListRowPtr PinnedItem)
		{
			if (!ValueAsString.IsEmpty())
			{
				FScopedTransaction Transaction(
					LOCTEXT("OnPropertyChangedTransaction", "Object Mixer - Bulk Edit Selected Row Properties") );
						
				for (const TSharedPtr<FObjectMixerEditorListRow>& SelectedRow : OtherSelectedItems)
				{
					const FObjectMixerEditorListRowPtr SelectedHybridRow = SelectedRow->GetHybridChild();
					const FObjectMixerEditorListRowPtr RowToUse = SelectedHybridRow.IsValid() ? SelectedHybridRow : SelectedRow;

					if (RowToUse != PinnedItem)
					{
						if (const TWeakPtr<IPropertyHandle>* SelectedHandlePtr = RowToUse->PropertyNamesToHandles.Find(PropertyName))
						{
							if (SelectedHandlePtr->IsValid())
							{
								if (UObject* ObjectToModify = RowToUse->GetObject())
								{
									ObjectToModify->Modify();
								}
										
								SelectedHandlePtr->Pin()->SetValueFromFormattedString(ValueAsString);
							}
						}
					}
				}
			}
		}
	};
	
	if (PropertyName != NAME_None)
	{
		if (const FObjectMixerEditorListRowPtr PinnedItem = GetHybridChildOrRowItemIfNull())
		{
			if (GetIsItemOrHybridChildSelected())
			{
				if (const TWeakPtr<IPropertyHandle>* HandlePtr = PinnedItem->PropertyNamesToHandles.Find(PropertyName); HandlePtr->IsValid())
				{
					if (TArray<FObjectMixerEditorListRowPtr> OtherSelectedItems = PinnedItem->GetSelectedTreeViewItems(); OtherSelectedItems.Num())
					{
						FString ValueAsString;
						if (TSharedPtr<IPropertyHandle> PinnedHandle = HandlePtr->Pin())
						{
							PinnedHandle->GetValueAsFormattedString(ValueAsString);
							Local::SetValueOnSelectedItems(ValueAsString, OtherSelectedItems, PropertyName, PinnedItem);
						}
					}
				}
			}
		}
	}
}

void SObjectMixerEditorListRow::OnClickBlueprintLink(UBlueprint* AsBlueprint, UObject* Object)
{
	if (AsBlueprint)
	{
		if (Object)
		{
			if (ensure(Object->GetClass()->ClassGeneratedBy == AsBlueprint))
			{
				AsBlueprint->SetObjectBeingDebugged(Object);
			}
		}
		// Open the blueprint
		GEditor->EditObject(AsBlueprint);
	}
}

FText SObjectMixerEditorListRow::GetHighlightText() const
{
	check (Item.IsValid());

	if (const TSharedPtr<SObjectMixerEditorList> PinnedListView = Item.Pin()->GetListViewPtr().Pin())
	{
		return PinnedListView->GetSearchTextFromSearchInputField();
	}

	return FText::GetEmpty();
}

#undef LOCTEXT_NAMESPACE
