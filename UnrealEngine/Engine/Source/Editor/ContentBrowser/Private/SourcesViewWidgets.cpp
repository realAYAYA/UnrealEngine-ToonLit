// Copyright Epic Games, Inc. All Rights Reserved.

#include "SourcesViewWidgets.h"

#include "AssetViewUtils.h"
#include "CollectionManagerTypes.h"
#include "CollectionViewTypes.h"
#include "CollectionViewUtils.h"
#include "Containers/UnrealString.h"
#include "ContentBrowserDataSource.h"
#include "ContentBrowserItem.h"
#include "ContentBrowserItemData.h"
#include "ContentBrowserPluginFilters.h"
#include "ContentBrowserUtils.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "DragAndDrop/CollectionDragDropOp.h"
#include "DragAndDrop/DecoratedDragDropOp.h"
#include "DragDropHandler.h"
#include "Fonts/SlateFontInfo.h"
#include "GenericPlatform/ICursor.h"
#include "Input/DragAndDrop.h"
#include "Layout/Children.h"
#include "Layout/Margin.h"
#include "Misc/EnumClassFlags.h"
#include "Misc/Optional.h"
#include "PathViewTypes.h"
#include "SAssetTagItem.h"
#include "SlotBase.h"
#include "Styling/AppStyle.h"
#include "Templates/Function.h"
#include "UObject/NameTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

struct FSlateBrush;

#define LOCTEXT_NAMESPACE "ContentBrowser"

//////////////////////////
// SAssetTreeItem
//////////////////////////

void SAssetTreeItem::Construct( const FArguments& InArgs )
{
	TreeItem = InArgs._TreeItem;
	OnNameChanged = InArgs._OnNameChanged;
	OnVerifyNameChanged = InArgs._OnVerifyNameChanged;
	IsItemExpanded = InArgs._IsItemExpanded;
	bDraggedOver = false;

	IsSelected = InArgs._IsSelected;

	FolderOpenBrush = FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderOpen");
	FolderClosedBrush = FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderClosed");
	FolderOpenCodeBrush = FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderOpenCode");
	FolderClosedCodeBrush = FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderClosedCode");
	FolderDeveloperBrush = FAppStyle::GetBrush("ContentBrowser.AssetTreeFolderDeveloper");
	
	FolderType = EFolderType::Normal;
	if (ContentBrowserUtils::IsItemDeveloperContent(InArgs._TreeItem->GetItem()))
	{
		FolderType = EFolderType::Developer;
	}
	else if (EnumHasAnyFlags(InArgs._TreeItem->GetItem().GetItemCategory(), EContentBrowserItemFlags::Category_Class))
	{
		FolderType = EFolderType::Code;
	}

	bool bIsRoot = !InArgs._TreeItem->Parent.IsValid();

	ChildSlot
	[
		SNew(SBorder)
		.BorderImage(this, &SAssetTreeItem::GetBorderImage)
		.Padding( FMargin( 0, bIsRoot ? 3 : 0, 0, 0 ) )	// For root items in the tree, give them a little breathing room on the top
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 2, 0)
			.VAlign(VAlign_Center)
			[
				// Folder Icon
				SNew(SImage) 
				.Image(this, &SAssetTreeItem::GetFolderIcon)
				.ColorAndOpacity(this, &SAssetTreeItem::GetFolderColor)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineRenameWidget, SInlineEditableTextBlock)
					.Text(this, &SAssetTreeItem::GetNameText)
					.ToolTipText(this, &SAssetTreeItem::GetToolTipText)
					.Font( InArgs._FontOverride.IsSet() ? InArgs._FontOverride : FAppStyle::GetFontStyle(bIsRoot ? "ContentBrowser.SourceTreeRootItemFont" : "ContentBrowser.SourceTreeItemFont") )
					.HighlightText( InArgs._HighlightText )
					.OnTextCommitted(this, &SAssetTreeItem::HandleNameCommitted)
					.OnVerifyTextChanged(this, &SAssetTreeItem::VerifyNameChanged)
					.IsSelected( InArgs._IsSelected )
					.IsReadOnly( this, &SAssetTreeItem::IsReadOnly )
			]
		]
	];

	if( InlineRenameWidget.IsValid() )
	{
		EnterEditingModeDelegateHandle = TreeItem.Pin()->OnRenameRequested().AddSP( InlineRenameWidget.Get(), &SInlineEditableTextBlock::EnterEditingMode );
	}
}

SAssetTreeItem::~SAssetTreeItem()
{
	if( InlineRenameWidget.IsValid() )
	{
		TreeItem.Pin()->OnRenameRequested().Remove( EnterEditingModeDelegateHandle );
	}
}

void SAssetTreeItem::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FTreeItem> TreeItemPinned = TreeItem.Pin();
	bDraggedOver = TreeItemPinned && DragDropHandler::HandleDragEnterItem(TreeItemPinned->GetItem(), DragDropEvent);
}

void SAssetTreeItem::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	if (TSharedPtr<FTreeItem> TreeItemPinned = TreeItem.Pin())
	{
		DragDropHandler::HandleDragLeaveItem(TreeItemPinned->GetItem(), DragDropEvent);
	}
	bDraggedOver = false;
}

FReply SAssetTreeItem::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FTreeItem> TreeItemPinned = TreeItem.Pin();
	bDraggedOver = TreeItemPinned && DragDropHandler::HandleDragOverItem(TreeItemPinned->GetItem(), DragDropEvent);
	return (bDraggedOver) ? FReply::Handled() : FReply::Unhandled();
}

FReply SAssetTreeItem::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FTreeItem> TreeItemPinned = TreeItem.Pin();
	if (TreeItemPinned && DragDropHandler::HandleDragDropOnItem(TreeItemPinned->GetItem(), DragDropEvent, AsShared()))
	{
		bDraggedOver = false;
		return FReply::Handled();
	}

	if (bDraggedOver)
	{
		// We were able to handle this operation, but could not due to another error - still report this drop as handled so it doesn't fall through to other widgets
		bDraggedOver = false;
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

bool SAssetTreeItem::VerifyNameChanged(const FText& InName, FText& OutError) const
{
	if ( TreeItem.IsValid() )
	{
		TSharedPtr<FTreeItem> TreeItemPtr = TreeItem.Pin();
		if(OnVerifyNameChanged.IsBound())
		{
			return OnVerifyNameChanged.Execute(TreeItemPtr, InName.ToString(), OutError);
		}
	}

	return true;
}

void SAssetTreeItem::HandleNameCommitted( const FText& NewText, ETextCommit::Type CommitInfo )
{
	if ( TreeItem.IsValid() )
	{
		TSharedPtr<FTreeItem> TreeItemPtr = TreeItem.Pin();

		if ( TreeItemPtr->IsNamingFolder() )
		{
			TreeItemPtr->SetNamingFolder(false);
		
			const FGeometry LastGeometry = GetTickSpaceGeometry();
			FVector2D MessageLoc;
			MessageLoc.X = LastGeometry.AbsolutePosition.X;
			MessageLoc.Y = LastGeometry.AbsolutePosition.Y + LastGeometry.Size.Y * LastGeometry.Scale;
		
			OnNameChanged.ExecuteIfBound(TreeItemPtr, NewText.ToString(), MessageLoc, CommitInfo);
		}
	}
}

bool SAssetTreeItem::IsReadOnly() const
{
	if ( TreeItem.IsValid() )
	{
		return !TreeItem.Pin()->IsNamingFolder();
	}
	else
	{
		return true;
	}
}

const FSlateBrush* SAssetTreeItem::GetFolderIcon() const
{
	switch( FolderType )
	{
	case EFolderType::Code:
		return ( IsItemExpanded.Get() ) ? FolderOpenCodeBrush : FolderClosedCodeBrush;

	case EFolderType::Developer:
		return FolderDeveloperBrush;

	default:
		return ( IsItemExpanded.Get() ) ? FolderOpenBrush : FolderClosedBrush;
	}
}

FSlateColor SAssetTreeItem::GetFolderColor() const
{
	if (TSharedPtr<FTreeItem> TreeItemPin = TreeItem.Pin())
	{
		FLinearColor FoundColor;
		FContentBrowserItemDataAttributeValue ColorAttributeValue = TreeItemPin->GetItem().GetItemAttribute(ContentBrowserItemAttributes::ItemColor);
		if (ColorAttributeValue.IsValid())
		{
			const FString ColorStr = ColorAttributeValue.GetValue<FString>();

			FLinearColor Color;
			if (Color.InitFromString(ColorStr))
			{
				FoundColor = Color;
			}
		}
		else
		{
			if (TSharedPtr<FLinearColor> Color = ContentBrowserUtils::LoadColor(TreeItemPin->GetItem().GetInvariantPath().ToString()))
			{
				FoundColor = *Color;
			}
		}

		return FoundColor;
	}
	
	return ContentBrowserUtils::GetDefaultColor();
}

FText SAssetTreeItem::GetNameText() const
{
	if (TSharedPtr<FTreeItem> TreeItemPin = TreeItem.Pin())
	{
		return TreeItemPin->GetItem().GetDisplayName();
	}
	return FText();
}

FText SAssetTreeItem::GetToolTipText() const
{
	if (TSharedPtr<FTreeItem> TreeItemPin = TreeItem.Pin())
	{
		return FText::FromName(TreeItemPin->GetItem().GetVirtualPath());
	}
	return FText();
}

const FSlateBrush* SAssetTreeItem::GetBorderImage() const
{
	static const FName NAME_DraggedBorderImage = TEXT("Menu.Background");
	static const FName NAME_NoBorderImage = TEXT("NoBorder");
	return bDraggedOver ? FAppStyle::GetBrush(NAME_DraggedBorderImage) : FAppStyle::GetBrush(NAME_NoBorderImage);
}



//////////////////////////
// SCollectionTreeItem
//////////////////////////

void SCollectionTreeItem::Construct( const FArguments& InArgs )
{
	ParentWidget = InArgs._ParentWidget;
	CollectionItem = InArgs._CollectionItem;
	OnBeginNameChange = InArgs._OnBeginNameChange;
	OnNameChangeCommit = InArgs._OnNameChangeCommit;
	OnVerifyRenameCommit = InArgs._OnVerifyRenameCommit;
	OnValidateDragDrop = InArgs._OnValidateDragDrop;
	OnHandleDragDrop = InArgs._OnHandleDragDrop;
	bDraggedOver = false;

	TSharedPtr<SAssetTagItem> AssetTagItem;

	ChildSlot
	[
		SAssignNew(AssetTagItem, SAssetTagItem)
		.ViewMode(EAssetTagItemViewMode::Compact)
		.BaseColor(this, &SCollectionTreeItem::GetCollectionColor)
		.DisplayName(this, &SCollectionTreeItem::GetNameText)
		.CountText(this, &SCollectionTreeItem::GetCollectionObjectCountText)
		.WarningText(this, &SCollectionTreeItem::GetCollectionWarningText)
		.HighlightText(InArgs._HighlightText)
		.OnBeginNameEdit(this, &SCollectionTreeItem::HandleBeginNameChange)
		.OnNameCommitted(this, &SCollectionTreeItem::HandleNameCommitted)
		.OnVerifyName(this, &SCollectionTreeItem::HandleVerifyNameChanged)
		.IsSelected(InArgs._IsSelected)
		.IsNameReadOnly(InArgs._IsReadOnly)
		.IsCheckBoxEnabled(InArgs._IsCheckBoxEnabled)
		.IsChecked(InArgs._IsCollectionChecked)
		.OnCheckStateChanged(InArgs._OnCollectionCheckStateChanged)
		.OnBuildToolTipInfo(this, &SCollectionTreeItem::BuildToolTipInfo)
	];

	// This is broadcast when the context menu / input binding requests a rename
	InArgs._CollectionItem->OnRenamedRequestEvent.AddSP(AssetTagItem.Get(), &SAssetTagItem::RequestRename);
}

void SCollectionTreeItem::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	// Cache this widget's geometry so it can pop up warnings over itself
	CachedGeometry = AllottedGeometry;
}

bool SCollectionTreeItem::ValidateDragDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent, bool& OutIsKnownDragOperation ) const
{
	OutIsKnownDragOperation = false;

	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();
	if (OnValidateDragDrop.IsBound() && CollectionItemPtr.IsValid())
	{
		return OnValidateDragDrop.Execute( CollectionItemPtr.ToSharedRef(), MyGeometry, DragDropEvent, OutIsKnownDragOperation );
	}

	return false;
}

void SCollectionTreeItem::OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	ValidateDragDrop(MyGeometry, DragDropEvent, bDraggedOver); // updates bDraggedOver
}

void SCollectionTreeItem::OnDragLeave( const FDragDropEvent& DragDropEvent )
{
	TSharedPtr<FDragDropOperation> Operation = DragDropEvent.GetOperation();
	if (Operation.IsValid())
	{
		Operation->SetCursorOverride(TOptional<EMouseCursor::Type>());

		if (Operation->IsOfType<FCollectionDragDropOp>() || Operation->IsOfType<FAssetDragDropOp>())
		{
			TSharedPtr<FDecoratedDragDropOp> DragDropOp = StaticCastSharedPtr<FDecoratedDragDropOp>(Operation);
			DragDropOp->ResetToDefaultToolTip();
		}
	}

	bDraggedOver = false;
}

FReply SCollectionTreeItem::OnDragOver( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	ValidateDragDrop(MyGeometry, DragDropEvent, bDraggedOver); // updates bDraggedOver
	return (bDraggedOver) ? FReply::Handled() : FReply::Unhandled();
}

FReply SCollectionTreeItem::OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent )
{
	if (ValidateDragDrop(MyGeometry, DragDropEvent, bDraggedOver) && OnHandleDragDrop.IsBound()) // updates bDraggedOver
	{
		bDraggedOver = false;
		return OnHandleDragDrop.Execute( CollectionItem.Pin().ToSharedRef(), MyGeometry, DragDropEvent );
	}

	if (bDraggedOver)
	{
		// We were able to handle this operation, but could not due to another error - still report this drop as handled so it doesn't fall through to other widgets
		bDraggedOver = false;
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SCollectionTreeItem::HandleBeginNameChange( const FText& OldText )
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		// If we get here via a context menu or input binding, bRenaming will already be set on the item.
		// If we got here by double clicking the editable text field, we need to set it now.
		CollectionItemPtr->bRenaming = true;

		OnBeginNameChange.ExecuteIfBound( CollectionItemPtr );
	}
}

void SCollectionTreeItem::HandleNameCommitted( const FText& NewText, ETextCommit::Type CommitInfo )
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		if ( CollectionItemPtr->bRenaming )
		{
			CollectionItemPtr->bRenaming = false;

			if ( OnNameChangeCommit.IsBound() )
			{
				FText WarningMessage;
				bool bIsCommitted = (CommitInfo != ETextCommit::OnCleared);
				if ( !OnNameChangeCommit.Execute(CollectionItemPtr, NewText.ToString(), bIsCommitted, WarningMessage) && ParentWidget.IsValid() && bIsCommitted )
				{
					// Failed to rename/create a collection, display a warning.
					ContentBrowserUtils::DisplayMessage(WarningMessage, CachedGeometry.GetLayoutBoundingRect(), ParentWidget.ToSharedRef());
				}
			}				
		}
	}
}

bool SCollectionTreeItem::HandleVerifyNameChanged( const FText& NewText, FText& OutErrorMessage )
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr.IsValid())
	{
		return !OnVerifyRenameCommit.IsBound() || OnVerifyRenameCommit.Execute(CollectionItemPtr, NewText.ToString(), CachedGeometry.GetLayoutBoundingRect(), OutErrorMessage);
	}

	return true;
}

FText SCollectionTreeItem::GetNameText() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		return FText::FromName(CollectionItemPtr->CollectionName);
	}
	else
	{
		return FText();
	}
}

FLinearColor SCollectionTreeItem::GetCollectionColor() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if ( CollectionItemPtr.IsValid() )
	{
		return CollectionItemPtr->CollectionColor;
	}
	
	return CollectionViewUtils::GetDefaultColor();
}

FText SCollectionTreeItem::GetCollectionObjectCountText() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr)
	{
		if (CollectionItemPtr->StorageMode == ECollectionStorageMode::Static)
		{
			return CollectionItemPtr->NumObjects > 999
				? NSLOCTEXT("ContentBrowser", "999+", "999+")
				: FText::AsNumber(CollectionItemPtr->NumObjects);
		}

		return NSLOCTEXT("ContentBrowser", "InfinitySymbol", "\u221E");
	}

	return FText::GetEmpty();
}

FText SCollectionTreeItem::GetCollectionWarningText() const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr)
	{
		switch (CollectionItemPtr->CurrentStatus)
		{
		case ECollectionItemStatus::IsOutOfDate:
			return NSLOCTEXT("ContentBrowser", "CollectionStatus_IsOutOfDate", "Collection is not at the latest revision");

		case ECollectionItemStatus::IsCheckedOutByAnotherUser:
			return NSLOCTEXT("ContentBrowser", "CollectionStatus_IsCheckedOutByAnotherUser", "Collection is checked out by another user");

		case ECollectionItemStatus::IsConflicted:
			return NSLOCTEXT("ContentBrowser", "CollectionStatus_IsConflicted", "Collection is conflicted - please use your external source control provider to resolve this conflict");

		case ECollectionItemStatus::IsMissingSCCProvider:
			return NSLOCTEXT("ContentBrowser", "CollectionStatus_IsMissingSCCProvider", "Collection is missing its source control provider - please check your source control settings");

		case ECollectionItemStatus::HasLocalChanges:
			return NSLOCTEXT("ContentBrowser", "CollectionStatus_HasLocalChanges", "Collection has local unsaved or uncomitted changes");

		default:
			break;
		}
	}

	return FText::GetEmpty();
}

void SCollectionTreeItem::BuildToolTipInfo(const FOnBuildAssetTagItemToolTipInfoEntry& InCallback) const
{
	TSharedPtr<FCollectionItem> CollectionItemPtr = CollectionItem.Pin();

	if (CollectionItemPtr)
	{
		InCallback(LOCTEXT("CollectionShareTypeKey", "Share Type"), ECollectionShareType::ToText(CollectionItemPtr->CollectionType));
		InCallback(LOCTEXT("CollectionStorageModeKey", "Storage Mode"), ECollectionStorageMode::ToText(CollectionItemPtr->StorageMode));
		if (CollectionItemPtr->StorageMode == ECollectionStorageMode::Static)
		{
			InCallback(LOCTEXT("CollectionObjectCountKey", "Object Count"), FText::AsNumber(CollectionItemPtr->NumObjects));
		}
	}
}

#undef LOCTEXT_NAMESPACE
