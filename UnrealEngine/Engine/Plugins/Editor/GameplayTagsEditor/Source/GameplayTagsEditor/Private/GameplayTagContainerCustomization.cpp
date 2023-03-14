// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayTagContainerCustomization.h"
#include "Widgets/Input/SComboButton.h"

#include "Widgets/Input/SButton.h"


#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "GameplayTagsEditorModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SHyperlink.h"
#include "EditorFontGlyphs.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "GameplayTagContainerCustomization"

TSharedRef<IPropertyTypeCustomization> FGameplayTagContainerCustomizationPublic::MakeInstance()
{
	return MakeInstanceWithOptions({});
}

TSharedRef<IPropertyTypeCustomization> FGameplayTagContainerCustomizationPublic::MakeInstanceWithOptions(const FGameplayTagContainerCustomizationOptions& Options)
{
	return MakeShareable(new FGameplayTagContainerCustomization(Options));
}

FGameplayTagContainerCustomization::FGameplayTagContainerCustomization(const FGameplayTagContainerCustomizationOptions& InOptions):
	Options(InOptions)
{}

void FGameplayTagContainerCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagContainerChanged = FSimpleDelegate::CreateSP(this, &FGameplayTagContainerCustomization::RefreshTagList);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagContainerChanged);

	FUIAction SearchForReferencesAction(FExecuteAction::CreateSP(this, &FGameplayTagContainerCustomization::OnWholeContainerSearchForReferences));

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(512)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SAssignNew(EditButton, SComboButton)
					.OnGetMenuContent(this, &FGameplayTagContainerCustomization::GetListContent)
					.OnMenuOpenChanged(this, &FGameplayTagContainerCustomization::OnGameplayTagListMenuOpenStateChanged)
					.ContentPadding(FMargin(2.0f, 2.0f))
					.MenuPlacement(MenuPlacement_BelowAnchor)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("GameplayTagContainerCustomization_Edit", "Edit..."))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.IsEnabled(!StructPropertyHandle->IsEditConst())
					.Text(LOCTEXT("GameplayTagContainerCustomization_Clear", "Clear All"))
					.OnClicked(this, &FGameplayTagContainerCustomization::OnClearAllButtonClicked)
					.Visibility(this, &FGameplayTagContainerCustomization::GetClearAllVisibility)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(4.0f)
				.Visibility(this, &FGameplayTagContainerCustomization::GetTagsListVisibility)
				[
					ActiveTags()
				]
			]
		]
		.AddCustomContextMenuAction(SearchForReferencesAction,
			LOCTEXT("WholeContainerSearchForReferences", "Search For References"),
			LOCTEXT("WholeContainerSearchForReferencesTooltip", "Find referencers that reference *any* of the tags in this container"),
			FSlateIcon());

	GEditor->RegisterForUndo(this);
}

TSharedRef<SWidget> FGameplayTagContainerCustomization::ActiveTags()
{	
	RefreshTagList();
	
	SAssignNew( TagListView, SListView<TSharedPtr<FGameplayTag>> )
	.ListItemsSource(&TagList)
	.SelectionMode(ESelectionMode::None)
	.OnGenerateRow(this, &FGameplayTagContainerCustomization::MakeListViewWidget);

	return TagListView->AsShared();
}

void FGameplayTagContainerCustomization::RefreshTagList()
{
	// Build the set of tags on any instance, collapsing common tags together
	TSet<FGameplayTag> CurrentTagSet;
	if (StructPropertyHandle.IsValid())
	{
		SGameplayTagWidget::EnumerateEditableTagContainersFromPropertyHandle(StructPropertyHandle.ToSharedRef(), [&CurrentTagSet](const SGameplayTagWidget::FEditableGameplayTagContainerDatum& EditableTagContainer)
		{
			if (const FGameplayTagContainer* Container = EditableTagContainer.TagContainer)
			{
				for (auto It = Container->CreateConstIterator(); It; ++It)
				{
					CurrentTagSet.Add(*It);
				}
			}
			return true;
		});
	}

	// Convert the set into pointers for the combo
	TagList.Empty(CurrentTagSet.Num());
	for (const FGameplayTag& CurrentTag : CurrentTagSet)
	{
		TagList.Add(MakeShared<FGameplayTag>(CurrentTag));
	}
	TagList.StableSort([](const TSharedPtr<FGameplayTag>& One, const TSharedPtr<FGameplayTag>& Two)
	{
		return *One < *Two;
	});

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FGameplayTagContainerCustomization::MakeListViewWidget(TSharedPtr<FGameplayTag> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	TSharedPtr<SWidget> TagItem;

	const FString TagName = Item->ToString();
	if (UGameplayTagsManager::Get().ShowGameplayTagAsHyperLinkEditor(TagName))
	{
		TagItem = SNew(SHyperlink)
			.Text(FText::FromString(TagName))
			.OnNavigate(this, &FGameplayTagContainerCustomization::OnTagDoubleClicked, *Item.Get());
	}
	else
	{
		TagItem = SNew(STextBlock)
			.Text(FText::FromString(TagName));
	}

	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
	[
		SNew(SBorder)
		.OnMouseButtonDown(this, &FGameplayTagContainerCustomization::OnSingleTagMouseButtonPressed, TagName)
		.Padding(0.0f)
		.BorderImage(FAppStyle::GetBrush("NoBorder"))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(0,0,2,0)
			[
				SNew(SButton)
				.IsEnabled(!StructPropertyHandle->IsEditConst())
				.ContentPadding(FMargin(0))
				.ButtonStyle(FAppStyle::Get(), "FlatButton.Danger")
				.ForegroundColor(FSlateColor::UseForeground())
				.OnClicked(this, &FGameplayTagContainerCustomization::OnRemoveTagClicked, *Item.Get())
				[
					SNew(STextBlock)
					.Font(FAppStyle::Get().GetFontStyle("FontAwesome.9"))
					.Text(FEditorFontGlyphs::Times)
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				TagItem.ToSharedRef()
			]
		]
	];
}

FReply FGameplayTagContainerCustomization::OnSingleTagMouseButtonPressed(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent, FString TagName)
{
	if (MouseEvent.IsMouseButtonDown(EKeys::RightMouseButton))
	{
		FMenuBuilder MenuBuilder(/*bShouldCloseWindowAfterMenuSelection=*/ true, /*CommandList=*/ nullptr);

		FUIAction SearchForReferencesAction(FExecuteAction::CreateSP(this, &FGameplayTagContainerCustomization::OnSingleTagSearchForReferences, TagName));

		MenuBuilder.BeginSection(NAME_None, FText::Format(LOCTEXT("SingleTagMenuHeading", "Tag Actions ({0})"), FText::AsCultureInvariant(TagName)));
		MenuBuilder.AddMenuEntry(
			LOCTEXT("SingleTagSearchForReferences", "Search For References"),
			FText::Format(LOCTEXT("SingleTagSearchForReferencesTooltip", "Find references to the tag {0}"), FText::AsCultureInvariant(TagName)),
			FSlateIcon(),
			SearchForReferencesAction);
		
		if (StructPropertyHandle)
		{
			FUIAction CopyAction = FUIAction(FExecuteAction::CreateSP(this, &FGameplayTagContainerCustomization::OnCopyTag, TagName));
			FUIAction PasteAction = FUIAction(FExecuteAction::CreateSP(this, &FGameplayTagContainerCustomization::OnPasteTag),FCanExecuteAction::CreateSP(this, &FGameplayTagContainerCustomization::CanPaste));
			
			if (CopyAction.IsBound() && PasteAction.IsBound())
			{
				FMenuEntryParams CopyContentParams;
				CopyContentParams.LabelOverride = NSLOCTEXT("PropertyView", "CopyProperty", "Copy");
				CopyContentParams.ToolTipOverride = FText::Format(LOCTEXT("SingleTagCopyTooltip", "Copy tag {0}"), FText::AsCultureInvariant(TagName));
				CopyContentParams.IconOverride = FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "GenericCommands.Copy");
				CopyContentParams.DirectActions = CopyAction;
				MenuBuilder.AddMenuEntry(CopyContentParams);

				FMenuEntryParams PasteContentParams;
				PasteContentParams.LabelOverride = NSLOCTEXT("PropertyView", "PasteProperty", "Paste");
				PasteContentParams.ToolTipOverride = FText::Format(LOCTEXT("SingleTagPasteTooltip", "Paste tag {0}"), FText::AsCultureInvariant(TagName));
				PasteContentParams.IconOverride = FSlateIcon(FCoreStyle::Get().GetStyleSetName(), "GenericCommands.Paste");
				PasteContentParams.DirectActions = PasteAction;
				MenuBuilder.AddMenuEntry(PasteContentParams);
			}
		}
		
		MenuBuilder.EndSection();

		// Spawn context menu
		FWidgetPath WidgetPath = MouseEvent.GetEventPath() != nullptr ? *MouseEvent.GetEventPath() : FWidgetPath();
		FSlateApplication::Get().PushMenu(TagListView.ToSharedRef(), WidgetPath, MenuBuilder.MakeWidget(), MouseEvent.GetScreenSpacePosition(), FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void FGameplayTagContainerCustomization::OnSingleTagSearchForReferences(FString TagName)
{
	FName TagFName(*TagName, FNAME_Find);
	if (FEditorDelegates::OnOpenReferenceViewer.IsBound() && !TagFName.IsNone())
	{
		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Emplace(FGameplayTag::StaticStruct(), TagFName);
		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
	}
}

void FGameplayTagContainerCustomization::OnWholeContainerSearchForReferences()
{
	if (FEditorDelegates::OnOpenReferenceViewer.IsBound())
	{
		TArray<FAssetIdentifier> AssetIdentifiers;
		AssetIdentifiers.Reserve(TagList.Num());
		for (const TSharedPtr<FGameplayTag>& TagPtr : TagList)
		{
			if (TagPtr->IsValid())
			{
				AssetIdentifiers.Emplace(FGameplayTag::StaticStruct(), TagPtr->GetTagName());
			}
		}

		FEditorDelegates::OnOpenReferenceViewer.Broadcast(AssetIdentifiers, FReferenceViewerParams());
	}
}

void FGameplayTagContainerCustomization::OnTagDoubleClicked(FGameplayTag Tag)
{
	UGameplayTagsManager::Get().NotifyGameplayTagDoubleClickedEditor(Tag.ToString());
}

FReply FGameplayTagContainerCustomization::OnRemoveTagClicked(FGameplayTag Tag)
{
	TArray<FString> NewValues;
	SGameplayTagWidget::EnumerateEditableTagContainersFromPropertyHandle(StructPropertyHandle.ToSharedRef(), [&Tag, &NewValues](const SGameplayTagWidget::FEditableGameplayTagContainerDatum& EditableTagContainer)
	{
		FGameplayTagContainer TagContainerCopy;
		if (const FGameplayTagContainer* Container = EditableTagContainer.TagContainer)
		{
			TagContainerCopy = *Container;
		}
		TagContainerCopy.RemoveTag(Tag);

		NewValues.Add(TagContainerCopy.ToString());
		return true;
	});

	{
		FScopedTransaction Transaction(LOCTEXT("RemoveGameplayTagFromContainer", "Remove Gameplay Tag"));
		StructPropertyHandle->SetPerObjectValues(NewValues);
	}

	RefreshTagList();

	return FReply::Handled();
}

TSharedRef<SWidget> FGameplayTagContainerCustomization::GetListContent()
{
	if (!StructPropertyHandle.IsValid() || StructPropertyHandle->GetProperty() == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	FString Categories = UGameplayTagsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);

	bool bReadOnly = StructPropertyHandle->IsEditConst();

	TSharedRef<SGameplayTagWidget> TagWidget = SNew(SGameplayTagWidget, TArray<SGameplayTagWidget::FEditableGameplayTagContainerDatum>()) // empty container list as built from StructPropertyHandle
		.Filter(Categories)
		.ReadOnly(bReadOnly)
		.TagContainerName(StructPropertyHandle->GetPropertyDisplayName().ToString())
		.OnTagChanged(this, &FGameplayTagContainerCustomization::RefreshTagList)
		.PropertyHandle(StructPropertyHandle)
		.ForceHideAddNewTag(Options.bForceHideAddTag)
		.ForceHideAddNewTagSource(Options.bForceHideAddTagSource);

	LastTagWidget = TagWidget;

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400)
		[
			TagWidget
		];
}

void FGameplayTagContainerCustomization::OnGameplayTagListMenuOpenStateChanged(bool bIsOpened)
{
	if (bIsOpened)
	{
		TSharedPtr<SGameplayTagWidget> TagWidget = LastTagWidget.Pin();
		if (TagWidget.IsValid())
		{
			EditButton->SetMenuContentWidgetToFocus(TagWidget->GetWidgetToFocusOnOpen());
		}
	}
}

FReply FGameplayTagContainerCustomization::OnClearAllButtonClicked()
{
	{
		FScopedTransaction Transaction(LOCTEXT("GameplayTagContainerCustomization_RemoveAllTags", "Remove All Gameplay Tags"));
		StructPropertyHandle->SetValueFromFormattedString(FGameplayTagContainer().ToString());
	}

	RefreshTagList();

	return FReply::Handled();
}

EVisibility FGameplayTagContainerCustomization::GetClearAllVisibility() const
{
	return TagList.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FGameplayTagContainerCustomization::GetTagsListVisibility() const
{
	return TagList.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

void FGameplayTagContainerCustomization::PostUndo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}
}

void FGameplayTagContainerCustomization::PostRedo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}
}

FGameplayTagContainerCustomization::~FGameplayTagContainerCustomization()
{
	// Forcibly close the popup to avoid crashes later
	if (EditButton.IsValid() && EditButton->IsOpen())
	{
		EditButton->SetIsOpen(false);
	}

	GEditor->UnregisterForUndo(this);
}

void FGameplayTagContainerCustomization::OnCopyTag(FString TagName)
{
	FPlatformApplicationMisc::ClipboardCopy(*TagName);
}

void FGameplayTagContainerCustomization::OnPasteTag()
{
	FString TagName;
	FPlatformApplicationMisc::ClipboardPaste(TagName);
	
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);

	if (Tag.IsValid())
	{
		TArray<FString> NewValues;
		SGameplayTagWidget::EnumerateEditableTagContainersFromPropertyHandle(StructPropertyHandle.ToSharedRef(), [&Tag, &NewValues](const SGameplayTagWidget::FEditableGameplayTagContainerDatum& EditableTagContainer)
		{
			FGameplayTagContainer TagContainerCopy;
			if (const FGameplayTagContainer* Container = EditableTagContainer.TagContainer)
			{
				TagContainerCopy = *Container;
			}
			TagContainerCopy.AddTag(Tag);

			NewValues.Add(TagContainerCopy.ToString());
			return true;
		});

		{
			FScopedTransaction Transaction(LOCTEXT("PasteGameplayTagFromContainer", "Paste Gameplay Tag"));
			StructPropertyHandle->SetPerObjectValues(NewValues);
		}

		RefreshTagList();
	}
}

bool FGameplayTagContainerCustomization::CanPaste()
{
	FString TagName;
	FPlatformApplicationMisc::ClipboardPaste(TagName);
	const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(TagName), false);

	return Tag.IsValid();
}

#undef LOCTEXT_NAMESPACE
