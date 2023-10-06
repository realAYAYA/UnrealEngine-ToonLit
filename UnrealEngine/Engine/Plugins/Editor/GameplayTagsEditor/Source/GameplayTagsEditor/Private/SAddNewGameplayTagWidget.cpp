// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAddNewGameplayTagWidget.h"
#include "DetailLayoutBuilder.h"
#include "GameplayTagsEditorModule.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsModule.h"
#include "Misc/Paths.h"
#include "Widgets/Input/SButton.h"
#include "Misc/MessageDialog.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SGridPanel.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Framework/Notifications/NotificationManager.h"

#define LOCTEXT_NAMESPACE "AddNewGameplayTagWidget"

SAddNewGameplayTagWidget::~SAddNewGameplayTagWidget()
{
	if (!GExitPurge)
	{
		IGameplayTagsModule::OnTagSettingsChanged.RemoveAll(this);
	}
}

void SAddNewGameplayTagWidget::Construct(const FArguments& InArgs)
{
	FText HintText = LOCTEXT("NewTagNameHint", "X.Y.Z");
	DefaultNewName = InArgs._NewTagName;
	if (DefaultNewName.IsEmpty() == false)
	{
		HintText = FText::FromString(DefaultNewName);
	}

	bAddingNewTag = false;
	bShouldGetKeyboardFocus = false;

	OnGameplayTagAdded = InArgs._OnGameplayTagAdded;
	IsValidTag = InArgs._IsValidTag;
	PopulateTagSources();

	IGameplayTagsModule::OnTagSettingsChanged.AddRaw(this, &SAddNewGameplayTagWidget::PopulateTagSources);

	ChildSlot
	[
		SNew(SBox)
		.Padding(InArgs._Padding)
		[
			SNew(SGridPanel)
			.FillColumn(1, 1.0)
			
			// Tag Name
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
				.Font(FAppStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont")))
				.OnTextCommitted(this, &SAddNewGameplayTagWidget::OnCommitNewTagName)
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
				.Font(FAppStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont")))
				.OnTextCommitted(this, &SAddNewGameplayTagWidget::OnCommitNewTagName)
			]

			// Tag Location
			+ SGridPanel::Slot(0, 2)
			.Padding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("CreateTagSource", "Source:"))
				.Font(FAppStyle::GetFontStyle( TEXT("PropertyWindow.NormalFont")))
			]
			+ SGridPanel::Slot(1, 2)
			.Padding(2)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Fill)
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				.VAlign(VAlign_Center)
				[
					SAssignNew(TagSourcesComboBox, SComboBox<TSharedPtr<FName> >)
					.OptionsSource(&TagSources)
					.OnGenerateWidget(this, &SAddNewGameplayTagWidget::OnGenerateTagSourcesComboBox)
					.ToolTipText(this, &SAddNewGameplayTagWidget::CreateTagSourcesComboBoxToolTip)
					.Content()
					[
						SNew(STextBlock)
						.Text(this, &SAddNewGameplayTagWidget::CreateTagSourcesComboBoxContent)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(2.0f, 0, 0, 0)
				.VAlign(VAlign_Center)
				[
					SNew( SButton )
					.ButtonStyle( FAppStyle::Get(), "NoBorder" )
					.Visibility(this, &SAddNewGameplayTagWidget::OnGetTagSourceFavoritesVisibility)
					.OnClicked(this, &SAddNewGameplayTagWidget::OnToggleTagSourceFavoriteClicked)
					.ToolTipText(LOCTEXT("ToggleFavoriteTooltip", "Toggle whether or not this tag source is your favorite source (new tags will go into your favorite source by default)"))
					.ContentPadding(0)
					[
						SNew(SImage)
						.Image(this, &SAddNewGameplayTagWidget::OnGetTagSourceFavoriteImage)
					]
				]
			]

			// Add Tag Button
			+ SGridPanel::Slot(0, 3)
			.ColumnSpan(2)
			.Padding(InArgs._AddButtonPadding)
			.HAlign(HAlign_Right)
			[
				SNew(SButton)
				.Text(LOCTEXT("AddNew", "Add New Tag"))
				.OnClicked(this, &SAddNewGameplayTagWidget::OnAddNewTagButtonPressed)
			]
		]
	];

	Reset(EResetType::ResetAll);
}

EVisibility SAddNewGameplayTagWidget::OnGetTagSourceFavoritesVisibility() const
{
	return (TagSources.Num() > 1) ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SAddNewGameplayTagWidget::OnToggleTagSourceFavoriteClicked()
{
	const bool bHasSelectedItem = TagSourcesComboBox.IsValid() && TagSourcesComboBox->GetSelectedItem().IsValid();
	const FName ActiveTagSource = bHasSelectedItem ? *TagSourcesComboBox->GetSelectedItem().Get() : FName();
	const bool bWasFavorite = FGameplayTagSource::GetFavoriteName() == ActiveTagSource;

	FGameplayTagSource::SetFavoriteName(bWasFavorite ? NAME_None : ActiveTagSource);

	return FReply::Handled();
}

const FSlateBrush* SAddNewGameplayTagWidget::OnGetTagSourceFavoriteImage() const
{
	const bool bHasSelectedItem = TagSourcesComboBox.IsValid() && TagSourcesComboBox->GetSelectedItem().IsValid();
	const FName ActiveTagSource = bHasSelectedItem ? *TagSourcesComboBox->GetSelectedItem().Get() : FName();
	const bool bIsFavoriteTagSource = FGameplayTagSource::GetFavoriteName() == ActiveTagSource;

	return FAppStyle::GetBrush(bIsFavoriteTagSource ? TEXT("Icons.Star") : TEXT("PropertyWindow.Favorites_Disabled"));
}

void SAddNewGameplayTagWidget::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bShouldGetKeyboardFocus)
	{
		bShouldGetKeyboardFocus = false;
		FSlateApplication::Get().SetKeyboardFocus(TagNameTextBox.ToSharedRef(), EFocusCause::SetDirectly);
		FSlateApplication::Get().SetUserFocus(0, TagNameTextBox.ToSharedRef());
	}
}

void SAddNewGameplayTagWidget::PopulateTagSources()
{
	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	TagSources.Empty();

	const FName DefaultSource = FGameplayTagSource::GetDefaultName();

	// Always ensure that the default source is first
	TagSources.Add( MakeShareable( new FName( DefaultSource ) ) );

	TArray<const FGameplayTagSource*> Sources;
	Manager.FindTagSourcesWithType(EGameplayTagSourceType::TagList, Sources);

	Algo::SortBy(Sources, &FGameplayTagSource::SourceName, FNameLexicalLess());

	for (const FGameplayTagSource* Source : Sources)
	{
		if (Source != nullptr && Source->SourceName != DefaultSource)
		{
			TagSources.Add(MakeShareable(new FName(Source->SourceName)));
		}
	}

	//Set selection to the latest added source
	if (TagSourcesComboBox != nullptr)
	{
		TagSourcesComboBox->SetSelectedItem(TagSources.Last());
	}	
}

void SAddNewGameplayTagWidget::Reset(EResetType ResetType)
{
	SetTagName();
	if (ResetType != EResetType::DoNotResetSource)
	{
		SelectTagSource(FGameplayTagSource::GetFavoriteName());
	}
	TagCommentTextBox->SetText(FText());
}

void SAddNewGameplayTagWidget::SetTagName(const FText& InName)
{
	TagNameTextBox->SetText(InName.IsEmpty() ? FText::FromString(DefaultNewName) : InName);
}

void SAddNewGameplayTagWidget::SelectTagSource(const FName& InSource)
{
	// Attempt to find the location in our sources, otherwise just use the first one
	int32 SourceIndex = INDEX_NONE;

	if (!InSource.IsNone())
	{
		for (int32 Index = 0; Index < TagSources.Num(); ++Index)
		{
			TSharedPtr<FName> Source = TagSources[Index];

			if (Source.IsValid() && *Source.Get() == InSource)
			{
				SourceIndex = Index;
				break;
			}
		}
	}

	if (SourceIndex != INDEX_NONE && TagSourcesComboBox.IsValid())
	{
		TagSourcesComboBox->SetSelectedItem(TagSources[SourceIndex]);
	}
}

void SAddNewGameplayTagWidget::OnCommitNewTagName(const FText& InText, ETextCommit::Type InCommitType)
{
	if (InCommitType == ETextCommit::OnEnter)
	{
		CreateNewGameplayTag();
	}
}

FReply SAddNewGameplayTagWidget::OnAddNewTagButtonPressed()
{
	CreateNewGameplayTag();
	return FReply::Handled();
}

void SAddNewGameplayTagWidget::AddSubtagFromParent(const FString& InParentTagName, const FName& InParentTagSource)
{
	const FText SubtagBaseName = !InParentTagName.IsEmpty() ? FText::Format(FText::FromString(TEXT("{0}.")), FText::FromString(InParentTagName)) : FText();

	SetTagName(SubtagBaseName);
	SelectTagSource(InParentTagSource);

	bShouldGetKeyboardFocus = true;
}

void SAddNewGameplayTagWidget::AddDuplicate(const FString& InParentTagName, const FName& InParentTagSource)
{
	SetTagName(FText::FromString(InParentTagName));
	SelectTagSource(InParentTagSource);

	bShouldGetKeyboardFocus = true;
}

void SAddNewGameplayTagWidget::CreateNewGameplayTag()
{
	if (NotificationItem.IsValid())
	{
		NotificationItem->SetVisibility(EVisibility::Collapsed);
	}

	const UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// Only support adding tags via ini file
	if (Manager.ShouldImportTagsFromINI() == false)
	{
		return;
	}

	if (TagSourcesComboBox->GetSelectedItem().Get() == nullptr)
	{
		FNotificationInfo Info(LOCTEXT("NoTagSource", "You must specify a source file for gameplay tags."));
		Info.ExpireDuration = 10.f;
		Info.bUseSuccessFailIcons = true;
		Info.Image = FAppStyle::GetBrush(TEXT("MessageLog.Error"));
		NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);
		
		return;
	}

	const FText TagNameAsText = TagNameTextBox->GetText();
	FString TagName = TagNameAsText.ToString();
	const FString TagComment = TagCommentTextBox->GetText().ToString();
	const FName TagSource = *TagSourcesComboBox->GetSelectedItem().Get();

	if (TagName.IsEmpty())
	{
		FNotificationInfo Info(LOCTEXT("NoTagName", "You must specify tag name."));
		Info.ExpireDuration = 10.f;
		Info.bUseSuccessFailIcons = true;
		Info.Image = FAppStyle::GetBrush(TEXT("MessageLog.Error"));
		NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

		return;
	}

	// check to see if this is a valid tag
	// first check the base rules for all tags then look for any additional rules in the delegate
	FText ErrorMsg;
	if (!UGameplayTagsManager::Get().IsValidGameplayTagString(TagName, &ErrorMsg) || 
		(IsValidTag.IsBound() && !IsValidTag.Execute(TagName, &ErrorMsg))
		)
	{
		FNotificationInfo Info(ErrorMsg);
		Info.ExpireDuration = 10.f;
		Info.bUseSuccessFailIcons = true;
		Info.Image = FAppStyle::GetBrush(TEXT("MessageLog.Error"));
		NotificationItem = FSlateNotificationManager::Get().AddNotification(Info);

		return;
	}

	// set bIsAddingNewTag, this guards against the window closing when it loses focus due to source control checking out a file
	TGuardValue<bool>	Guard(bAddingNewTag, true);

	IGameplayTagsEditorModule::Get().AddNewGameplayTagToINI(TagName, TagComment, TagSource);

	OnGameplayTagAdded.ExecuteIfBound(TagName, TagComment, TagSource);

	Reset(EResetType::DoNotResetSource);
}

TSharedRef<SWidget> SAddNewGameplayTagWidget::OnGenerateTagSourcesComboBox(TSharedPtr<FName> InItem)
{
	return SNew(STextBlock)
		.Text(FText::FromName(*InItem.Get()));
}

FText SAddNewGameplayTagWidget::CreateTagSourcesComboBoxContent() const
{
	const bool bHasSelectedItem = TagSourcesComboBox.IsValid() && TagSourcesComboBox->GetSelectedItem().IsValid();

	return bHasSelectedItem ? FText::FromName(*TagSourcesComboBox->GetSelectedItem().Get()) : LOCTEXT("NewTagLocationNotSelected", "Not selected");
}

FText SAddNewGameplayTagWidget::CreateTagSourcesComboBoxToolTip() const
{
	const bool bHasSelectedItem = TagSourcesComboBox.IsValid() && TagSourcesComboBox->GetSelectedItem().IsValid();

	if (bHasSelectedItem)
	{
		UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		const FGameplayTagSource* Source = Manager.FindTagSource(*TagSourcesComboBox->GetSelectedItem().Get());
		if (Source)
		{
			FString FilePath = Source->GetConfigFileName();

			if (FPaths::IsUnderDirectory(FilePath, FPaths::ProjectDir()))
			{
				FPaths::MakePathRelativeTo(FilePath, *FPaths::ProjectDir());
			}
			return FText::FromString(FilePath);
		}
	}

	return FText();
}

#undef LOCTEXT_NAMESPACE
