// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAddNewRestrictedGameplayTagWidget.h"
#include "DetailLayoutBuilder.h"
#include "Framework/Views/TableViewMetadata.h"
#include "GameplayTagsEditorModule.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsModule.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SGridPanel.h"


#define LOCTEXT_NAMESPACE "AddNewRestrictedGameplayTagWidget"

SAddNewRestrictedGameplayTagWidget::~SAddNewRestrictedGameplayTagWidget()
{
	if (!GExitPurge)
	{
		IGameplayTagsModule::OnTagSettingsChanged.RemoveAll(this);
	}
}

void SAddNewRestrictedGameplayTagWidget::Construct(const FArguments& InArgs)
{
	FText HintText = LOCTEXT("NewTagNameHint", "X.Y.Z");
	DefaultNewName = InArgs._NewRestrictedTagName;
	if (DefaultNewName.IsEmpty() == false)
	{
		HintText = FText::FromString(DefaultNewName);
	}

	bAddingNewRestrictedTag = false;
	bShouldGetKeyboardFocus = false;

	OnRestrictedGameplayTagAdded = InArgs._OnRestrictedGameplayTagAdded;
	PopulateTagSources();

	IGameplayTagsModule::OnTagSettingsChanged.AddRaw(this, &SAddNewRestrictedGameplayTagWidget::PopulateTagSources);

	ChildSlot
	[
		SNew(SBox)
		.Padding(InArgs._Padding)
		[
			SNew(SGridPanel)
			.FillColumn(1, 1.0)

			// Restricted Tag Name
			+ SGridPanel::Slot(0, 0)
			.Padding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("NewTagName", "Name:"))
			]
			+ SGridPanel::Slot(1, 0)
			.Padding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(TagNameTextBox, SEditableTextBox)
				.HintText(HintText)
				.OnTextCommitted(this, &SAddNewRestrictedGameplayTagWidget::OnCommitNewTagName)
			]
			
			// Tag Comment
			+ SGridPanel::Slot(0, 1)
			.Padding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Font(FAppStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont")))
				.Text(LOCTEXT("TagComment", "Comment:"))
			]
			+ SGridPanel::Slot(1, 1)
			.Padding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(TagCommentTextBox, SEditableTextBox)
				.HintText(LOCTEXT("TagCommentHint", "Comment"))
				.OnTextCommitted(this, &SAddNewRestrictedGameplayTagWidget::OnCommitNewTagName)
			]

			// Allow non-restricted children
			+ SGridPanel::Slot(0, 2)
			.Padding(2)
			.ColumnSpan(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(FAppStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont")))
					.Text(LOCTEXT("AllowNonRestrictedChildren", "Allow non-restricted children:"))
				]
				+ SHorizontalBox::Slot()
				.Padding(FMargin(4,0))
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SAssignNew(AllowNonRestrictedChildrenCheckBox, SCheckBox)
				]
			]

			// Tag Location
			+ SGridPanel::Slot(0, 3)
			.Padding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CreateTagSource", "Source:"))
				.Font(IDetailLayoutBuilder::GetDetailFont())
			]
			+ SGridPanel::Slot(1, 3)
			.Padding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				SAssignNew(TagSourcesComboBox, SComboBox<TSharedPtr<FName> >)
				.OptionsSource(&RestrictedTagSources)
				.OnGenerateWidget(this, &SAddNewRestrictedGameplayTagWidget::OnGenerateTagSourcesComboBox)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &SAddNewRestrictedGameplayTagWidget::CreateTagSourcesComboBoxContent)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]

			// Add Tag Button
			+ SGridPanel::Slot(0, 4)
			.ColumnSpan(2)
			.Padding(FMargin(0, 16))
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddNew", "Add New Tag"))
				.OnClicked(this, &SAddNewRestrictedGameplayTagWidget::OnAddNewTagButtonPressed)
			]
		]
	];
			
	Reset();
}

void SAddNewRestrictedGameplayTagWidget::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bShouldGetKeyboardFocus)
	{
		bShouldGetKeyboardFocus = false;
		FSlateApplication::Get().SetKeyboardFocus(TagNameTextBox.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

void SAddNewRestrictedGameplayTagWidget::PopulateTagSources()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	RestrictedTagSources.Empty();

	TArray<const FGameplayTagSource*> Sources;
	Manager.GetRestrictedTagSources(Sources);

	// Used to make sure we have a non-empty list of restricted tag sources. Not an actual source.
	const FName PlaceholderSource = NAME_None;

	// Add the placeholder source if no other sources exist
	if (Sources.Num() == 0)
	{
		RestrictedTagSources.Add(MakeShareable(new FName(PlaceholderSource)));
	}

	for (const FGameplayTagSource* Source : Sources)
	{
		if (Source != nullptr && Source->SourceName != PlaceholderSource)
		{
			RestrictedTagSources.Add(MakeShareable(new FName(Source->SourceName)));
		}
	}
}

void SAddNewRestrictedGameplayTagWidget::Reset(FName TagSource)
{
	SetTagName();
	SelectTagSource(TagSource);
	SetAllowNonRestrictedChildren();
	TagCommentTextBox->SetText(FText());
}

void SAddNewRestrictedGameplayTagWidget::SetTagName(const FText& InName)
{
	TagNameTextBox->SetText(InName.IsEmpty() ? FText::FromString(DefaultNewName) : InName);
}

void SAddNewRestrictedGameplayTagWidget::SelectTagSource(const FName& InSource)
{
	// Attempt to find the location in our sources, otherwise just use the first one
	int32 SourceIndex = 0;

	if (!InSource.IsNone())
	{
		for (int32 Index = 0; Index < RestrictedTagSources.Num(); ++Index)
		{
			TSharedPtr<FName> Source = RestrictedTagSources[Index];

			if (Source.IsValid() && *Source.Get() == InSource)
			{
				SourceIndex = Index;
				break;
			}
		}
	}

	TagSourcesComboBox->SetSelectedItem(RestrictedTagSources[SourceIndex]);
}

void SAddNewRestrictedGameplayTagWidget::SetAllowNonRestrictedChildren(bool bInAllowNonRestrictedChildren)
{
	AllowNonRestrictedChildrenCheckBox->SetIsChecked(bInAllowNonRestrictedChildren ? ECheckBoxState::Checked : ECheckBoxState::Unchecked);
}

void SAddNewRestrictedGameplayTagWidget::OnCommitNewTagName(const FText& InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter)
	{
		ValidateNewRestrictedTag();
	}
}

FReply SAddNewRestrictedGameplayTagWidget::OnAddNewTagButtonPressed()
{
	ValidateNewRestrictedTag();
	return FReply::Handled();
}

void SAddNewRestrictedGameplayTagWidget::AddSubtagFromParent(const FString& ParentTagName, const FName& ParentTagSource, bool bAllowNonRestrictedChildren)
{
	FText SubtagBaseName = !ParentTagName.IsEmpty() ? FText::Format(FText::FromString(TEXT("{0}.")), FText::FromString(ParentTagName)) : FText();

	SetTagName(SubtagBaseName);
	SelectTagSource(ParentTagSource);
	SetAllowNonRestrictedChildren(bAllowNonRestrictedChildren);

	bShouldGetKeyboardFocus = true;
}

void SAddNewRestrictedGameplayTagWidget::AddDuplicate(const FString& ParentTagName, const FName& ParentTagSource, bool bAllowNonRestrictedChildren)
{
	SetTagName(FText::FromString(ParentTagName));
	SelectTagSource(ParentTagSource);
	SetAllowNonRestrictedChildren(bAllowNonRestrictedChildren);

	bShouldGetKeyboardFocus = true;
}

void SAddNewRestrictedGameplayTagWidget::ValidateNewRestrictedTag()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	FString TagName = TagNameTextBox->GetText().ToString();
	FString TagComment = TagCommentTextBox->GetText().ToString();
	const FName TagSource = *TagSourcesComboBox->GetSelectedItem().Get();

	if (TagSource == NAME_None)
	{
		FNotificationInfo Info(LOCTEXT("NoRestrictedSource", "You must specify a source file for restricted gameplay tags."));
		Info.ExpireDuration = 10.f;
		Info.bUseSuccessFailIcons = true;
		Info.Image = FAppStyle::GetBrush(TEXT("MessageLog.Error"));

		AddRestrictedGameplayTagDialog = FSlateNotificationManager::Get().AddNotification(Info);

		return;
	}

	TArray<FString> TagSourceOwners;
	Manager.GetOwnersForTagSource(TagSource.ToString(), TagSourceOwners);

	bool bHasOwner = false;
	for (const FString& Owner : TagSourceOwners)
	{
		if (!Owner.IsEmpty())
		{
			bHasOwner = true;
			break;
		}
	}

	if (bHasOwner)
	{
		// check if we're one of the owners; if we are then we don't need to pop up the permission dialog
		bool bRequiresPermission = true;
		const FString& UserName = FPlatformProcess::UserName();
		for (const FString& Owner : TagSourceOwners)
		{
			if (Owner.Equals(UserName))
			{
				CreateNewRestrictedGameplayTag();
				bRequiresPermission = false;
			}
		}

		if (bRequiresPermission)
		{
			FString StringToDisplay = TEXT("Do you have permission from ");
			StringToDisplay.Append(TagSourceOwners[0]);
			for (int Idx = 1; Idx < TagSourceOwners.Num(); ++Idx)
			{
				StringToDisplay.Append(TEXT(" or "));
				StringToDisplay.Append(TagSourceOwners[Idx]);
			}
			StringToDisplay.Append(TEXT(" to modify "));
			StringToDisplay.Append(TagSource.ToString());
			StringToDisplay.Append(TEXT("?"));

			FNotificationInfo Info(FText::FromString(StringToDisplay));
			Info.ExpireDuration = 10.f;
			Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("RestrictedTagPopupButtonAccept", "Yes"), FText(), FSimpleDelegate::CreateSP(this, &SAddNewRestrictedGameplayTagWidget::CreateNewRestrictedGameplayTag), SNotificationItem::CS_None));
			Info.ButtonDetails.Add(FNotificationButtonInfo(LOCTEXT("RestrictedTagPopupButtonReject", "No"), FText(), FSimpleDelegate::CreateSP(this, &SAddNewRestrictedGameplayTagWidget::CancelNewTag), SNotificationItem::CS_None));

			AddRestrictedGameplayTagDialog = FSlateNotificationManager::Get().AddNotification(Info);
		}
	}
	else
	{
		CreateNewRestrictedGameplayTag();
	}
}

void SAddNewRestrictedGameplayTagWidget::CreateNewRestrictedGameplayTag()
{
	if (AddRestrictedGameplayTagDialog.IsValid())
	{
		AddRestrictedGameplayTagDialog->SetVisibility(EVisibility::Collapsed);
	}

	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// Only support adding tags via ini file
	if (Manager.ShouldImportTagsFromINI() == false)
	{
		return;
	}

	const FString TagName = TagNameTextBox->GetText().ToString();
	const FString TagComment = TagCommentTextBox->GetText().ToString();
	const bool bAllowNonRestrictedChildren = AllowNonRestrictedChildrenCheckBox->IsChecked();
	const FName TagSource = *TagSourcesComboBox->GetSelectedItem().Get();

	if (TagName.IsEmpty())
	{
		return;
	}

	// set bIsAddingNewTag, this guards against the window closing when it loses focus due to source control checking out a file
	TGuardValue<bool>	Guard(bAddingNewRestrictedTag, true);

	IGameplayTagsEditorModule::Get().AddNewGameplayTagToINI(TagName, TagComment, TagSource, true, bAllowNonRestrictedChildren);

	OnRestrictedGameplayTagAdded.ExecuteIfBound(TagName, TagComment, TagSource);

	Reset(TagSource);
}

void SAddNewRestrictedGameplayTagWidget::CancelNewTag()
{
	if (AddRestrictedGameplayTagDialog.IsValid())
	{
		AddRestrictedGameplayTagDialog->SetVisibility(EVisibility::Collapsed);
	}
}

TSharedRef<SWidget> SAddNewRestrictedGameplayTagWidget::OnGenerateTagSourcesComboBox(TSharedPtr<FName> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromName(*InItem.Get()));
}

FText SAddNewRestrictedGameplayTagWidget::CreateTagSourcesComboBoxContent() const
{
	const bool bHasSelectedItem = TagSourcesComboBox.IsValid() && TagSourcesComboBox->GetSelectedItem().IsValid();

	return bHasSelectedItem ? FText::FromName(*TagSourcesComboBox->GetSelectedItem().Get()) : LOCTEXT("NewTagLocationNotSelected", "Not selected");
}

#undef LOCTEXT_NAMESPACE
