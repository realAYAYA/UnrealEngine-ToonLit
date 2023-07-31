// Copyright Epic Games, Inc. All Rights Reserved.

#include "Views/Widgets/SCollectionSelectionButton.h"

#include "ObjectMixerEditorSerializedData.h"
#include "Views/List/ObjectMixerEditorListFilters/ObjectMixerEditorListFilter_Collection.h"
#include "Views/MainPanel/ObjectMixerEditorMainPanel.h"
#include "Views/MainPanel/SObjectMixerEditorMainPanel.h"

#include "DragAndDrop/DecoratedDragDropOp.h"
#include "Framework/Commands/GenericCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Styling/AppStyle.h"
#include "Views/List/ObjectMixerEditorListRow.h"
#include "Widgets/Input/SMenuAnchor.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "ObjectMixerEditor"

class FCollectionSelectionButtonDragDropOp : public FDecoratedDragDropOp
{
public:
	DRAG_DROP_OPERATOR_TYPE(FCollectionSelectionButtonDragDropOp, FDecoratedDragDropOp)

	/** The item being dragged and dropped */
	FName DraggedItem;

	/** Constructs a new drag/drop operation */
	static TSharedRef<FCollectionSelectionButtonDragDropOp> New(const FName DraggedItem)
	{
		TSharedRef<FCollectionSelectionButtonDragDropOp> Operation = MakeShareable(
			   new FCollectionSelectionButtonDragDropOp());
		if (DraggedItem != NAME_None)
		{
			Operation->DraggedItem = DraggedItem;

			Operation->DefaultHoverIcon = FAppStyle::Get().GetBrush("Graph.ConnectorFeedback.Error");

			Operation->DefaultHoverText = LOCTEXT("DefaultCollectionButtonHoverText","Drop onto another Collection Button to set a custom order.");

			Operation->Construct();
		}

		return Operation;
	}
};

void SCollectionSelectionButton::Construct(
	const FArguments& InArgs,
	const TSharedRef<SObjectMixerEditorMainPanel> MainPanelWidget,
	const TSharedRef<FObjectMixerEditorListFilter_Collection> InCollectionListFilter)
{
	MainPanelPtr = MainPanelWidget;
	CollectionListFilter = InCollectionListFilter;
	
	ChildSlot
	[
		SAssignNew(MenuAnchor, SMenuAnchor)
		.OnGetMenuContent(this, &SCollectionSelectionButton::GetContextMenu)
		[
			SNew(SBorder)
			.Padding(FMargin(16, 4))
			.BorderImage(this, &SCollectionSelectionButton::GetBorderBrush)
			.ForegroundColor(this, &SCollectionSelectionButton::GetBorderForeground)
			[
				SAssignNew(EditableTextBlock, SInlineEditableTextBlock)
				.Style(&FAppStyle::Get().GetWidgetStyle<FInlineEditableTextBlockStyle>("InlineEditableTextBlockSmallStyle"))
				.Text_Lambda([this, PinnedFilter = CollectionListFilter.Pin()]() { return FText::FromName(PinnedFilter->CollectionName);})
				.Visibility(EVisibility::HitTestInvisible)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.OnTextCommitted(this, &SCollectionSelectionButton::OnEditableTextCommitted)
			]
		]
	];
}

TSharedRef<SWidget> SCollectionSelectionButton::GetContextMenu() const
{
	FMenuBuilder MenuBuilder(true, nullptr);

	const FName CollectionName = CollectionListFilter.Pin()->CollectionName;

	MenuBuilder.AddMenuEntry(
		FText::Format(LOCTEXT("DeleteCollectionButtonMenuEntry", "Delete '{0}'"), FText::FromName(CollectionName)),
		LOCTEXT("DeleteCollectionButtonMenuEntryTooltip", "Remove this collection. This operation can be undone."),
		FGenericCommands::Get().Delete->GetIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			MainPanelPtr.Pin()->RequestRemoveCollection(CollectionListFilter.Pin()->CollectionName);
		}))
	);

	MenuBuilder.AddMenuEntry(
		FText::Format(LOCTEXT("DuplicateCollectionButtonMenuEntry", "Duplicate '{0}'"), FText::FromName(CollectionName)),
		LOCTEXT("DuplicateCollectionButtonMenuEntryTooltip", "Duplicate this collection. This operation can be undone."),
		FGenericCommands::Get().Duplicate->GetIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			const TSharedPtr<FObjectMixerEditorListFilter_Collection> CollectionList = CollectionListFilter.Pin();
			FName DuplicateName = CollectionList->CollectionName;
			MainPanelPtr.Pin()->RequestDuplicateCollection(CollectionList->CollectionName, DuplicateName);
		}))
	);

	MenuBuilder.AddMenuEntry(
		FText::Format(LOCTEXT("RenameCollectionButtonMenuEntry", "Rename '{0}'"), FText::FromName(CollectionName)),
		LOCTEXT("RenameCollectionButtonMenuEntryTooltip", "Rename this collection. This operation can be undone."),
		FGenericCommands::Get().Rename->GetIcon(),
		FUIAction(FExecuteAction::CreateLambda([this]()
		{
			EditableTextBlock->EnterEditingMode();
		}))
	);
			
	return MenuBuilder.MakeWidget();
}

void SCollectionSelectionButton::OnEditableTextCommitted(const FText& Text, ETextCommit::Type Type)
{
	if (Type == ETextCommit::OnEnter)
	{
		const FName NewCollectionName = *Text.ToString();
		const TSharedPtr<FObjectMixerEditorListFilter_Collection> CollectionFilter = CollectionListFilter.Pin();
		MainPanelPtr.Pin()->RequestRenameCollection(CollectionFilter->CollectionName, NewCollectionName);
	}
}

FReply SCollectionSelectionButton::OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) 
{
	bIsPressed = true;
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		return FReply::Handled().DetectDrag( SharedThis(this), EKeys::LeftMouseButton );
	}
	return FReply::Handled();
}

FReply SCollectionSelectionButton::OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) 
{
	bIsPressed = false;
	const FName CollectionName = CollectionListFilter.Pin()->CollectionName;
	if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton )
	{
		MainPanelPtr.Pin()->OnCollectionCheckedStateChanged(!GetIsChecked(), CollectionName);
	}
	if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (CollectionName != UObjectMixerEditorSerializedData::AllCollectionName && CollectionName != NAME_None)
		{
			MenuAnchor->SetIsOpen(true);
		}
	}
	return FReply::Handled();
}

FReply SCollectionSelectionButton::OnDragDetected(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	const FName CollectionName = CollectionListFilter.Pin()->CollectionName;
	if (CollectionName != UObjectMixerEditorSerializedData::AllCollectionName && CollectionName != NAME_None)
	{
		TSharedRef<FCollectionSelectionButtonDragDropOp> OperationFromCollection =
		   FCollectionSelectionButtonDragDropOp::New(CollectionName);

		OperationFromCollection->ResetToDefaultToolTip();

		bDropIsValid = false;

		return FReply::Handled().BeginDragDrop(OperationFromCollection);
	}

	return FReply::Handled();
}

void SCollectionSelectionButton::OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	const FName CollectionName = CollectionListFilter.Pin()->CollectionName;
	
	if (TSharedPtr<FObjectMixerListRowDragDropOp> OperationFromRow =
		DragDropEvent.GetOperationAs<FObjectMixerListRowDragDropOp>())
	{
		if (CollectionName != UObjectMixerEditorSerializedData::AllCollectionName && CollectionName != NAME_None)
		{
			OperationFromRow->SetToolTip(
			   LOCTEXT("DropRowItemsOntoCollectionButtonCTA","Add selected items to this collection"),
			   FAppStyle::Get().GetBrush("Graph.ConnectorFeedback.OK"));

			bDropIsValid = true;
		}
	}

	if (TSharedPtr<FCollectionSelectionButtonDragDropOp> OperationFromCollection =
		DragDropEvent.GetOperationAs<FCollectionSelectionButtonDragDropOp>())
	{
		if (CollectionName != UObjectMixerEditorSerializedData::AllCollectionName &&
			CollectionName != NAME_None && CollectionName != OperationFromCollection->DraggedItem)
		{
			OperationFromCollection->SetToolTip(
			   FText::Format(
				LOCTEXT("DropCollectionButtonOntoCollectionButtonCTA_Format","Reorder {0} before {1}"),
				FText::FromName(OperationFromCollection->DraggedItem), FText::FromName(CollectionName)),
			   FAppStyle::Get().GetBrush("Graph.ConnectorFeedback.OK"));

			bDropIsValid = true;
		}
	}
}

void SCollectionSelectionButton::OnDragLeave(const FDragDropEvent& DragDropEvent)
{
	bIsPressed = false;
	bDropIsValid = false;
	
	if (TSharedPtr<FObjectMixerListRowDragDropOp> OperationFromRow =
		DragDropEvent.GetOperationAs<FObjectMixerListRowDragDropOp>())
	{
		OperationFromRow->ResetToDefaultToolTip();
	}

	if (TSharedPtr<FCollectionSelectionButtonDragDropOp> OperationFromCollection =
		DragDropEvent.GetOperationAs<FCollectionSelectionButtonDragDropOp>())
	{
		OperationFromCollection->ResetToDefaultToolTip();
	}
}

FReply SCollectionSelectionButton::OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
{
	bIsPressed = false;
	bDropIsValid = false;
	
	const FName CollectionName = CollectionListFilter.Pin()->CollectionName;
	
	if (TSharedPtr<FObjectMixerListRowDragDropOp> OperationFromRow =
		DragDropEvent.GetOperationAs<FObjectMixerListRowDragDropOp>())
	{
		if (CollectionName != UObjectMixerEditorSerializedData::AllCollectionName && CollectionName != NAME_None)
		{
			if (const TSharedPtr<FObjectMixerEditorMainPanel> MainPanelModel = MainPanelPtr.Pin()->GetMainPanelModel().Pin())
			{
				TSet<FSoftObjectPath> ObjectPaths;

				for (const TSharedPtr<FObjectMixerEditorListRow>& Item : OperationFromRow->DraggedItems)
				{
					if (const UObject* Object = Item->GetObject())
					{
						ObjectPaths.Add(Object);
					}
				}
	
				MainPanelModel->RequestAddObjectsToCollection(CollectionName, ObjectPaths);
			}
		}
	}

	if (TSharedPtr<FCollectionSelectionButtonDragDropOp> OperationFromCollection =
		DragDropEvent.GetOperationAs<FCollectionSelectionButtonDragDropOp>())
	{
		if (CollectionName != UObjectMixerEditorSerializedData::AllCollectionName &&
			CollectionName != NAME_None && CollectionName != OperationFromCollection->DraggedItem)
		{
			if (const TSharedPtr<FObjectMixerEditorMainPanel> MainPanelModel = MainPanelPtr.Pin()->GetMainPanelModel().Pin())
			{
				MainPanelModel->RequestReorderCollection(
					OperationFromCollection->DraggedItem, CollectionName);
			}
		}
	}
		
	return FReply::Handled();
}

const FSlateBrush* SCollectionSelectionButton::GetBorderBrush() const
{
	if (GetIsChecked())
	{
		if (bIsPressed)
		{
			return &CheckedPressedImage;
		}
		else if (IsHovered())
		{
			return &CheckedHoveredImage;
		}

		return &CheckedImage;
	}

	if (bIsPressed)
	{
		return &UncheckedPressedImage;
	}
	else if (IsHovered())
	{
		return &UncheckedHoveredImage;
	}

	return &UncheckedImage;
			
}

FSlateColor SCollectionSelectionButton::GetBorderForeground() const
{
	if (GetIsChecked() || bIsPressed || IsHovered())
	{
		return FStyleColors::White;
	}

	return FStyleColors::Foreground;
}

bool SCollectionSelectionButton::GetIsChecked() const
{
	return CollectionListFilter.Pin()->GetIsFilterActive();
}

#undef LOCTEXT_NAMESPACE
