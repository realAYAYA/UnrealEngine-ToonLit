// Copyright Epic Games, Inc. All Rights Reserved.

#include "SAddNewGameplayTagSourceWidget.h"
#include "DetailLayoutBuilder.h"
#include "GameplayTagsSettings.h"
#include "GameplayTagsEditorModule.h"
#include "GameplayTagsModule.h"
#include "Widgets/Input/SButton.h"

#define LOCTEXT_NAMESPACE "AddNewGameplayTagSourceWidget"

void SAddNewGameplayTagSourceWidget::Construct(const FArguments& InArgs)
{
	FText HintText = LOCTEXT("NewSourceNameHint", "SourceName.ini");
	DefaultNewName = InArgs._NewSourceName;
	if (DefaultNewName.IsEmpty() == false)
	{
		HintText = FText::FromString(DefaultNewName);
	}

	bShouldGetKeyboardFocus = false;

	OnGameplayTagSourceAdded = InArgs._OnGameplayTagSourceAdded;
	PopulateTagRoots();

	ChildSlot
	[
		SNew(SVerticalBox)

		// Tag Name
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 4.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("NewSourceName", "Name:"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(SourceNameTextBox, SEditableTextBox)
				.MinDesiredWidth(240.0f)
				.HintText(HintText)
			]
		]

		// Tag source root
		+ SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.Padding(2.0f, 6.0f)
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ConfigPathLabel", "Config Path:"))
				.ToolTipText(LOCTEXT("RootPathTooltip", "Set the base config path for added source, this includes paths from plugins and other places that call AddTagIniSearchPath"))
			]

			+ SHorizontalBox::Slot()
			.Padding(2.0f, 2.0f)
			.FillWidth(1.0f)
			.HAlign(HAlign_Right)
			[
				SAssignNew(TagRootsComboBox, SComboBox<TSharedPtr<FString> >)
				.OptionsSource(&TagRoots)
				.OnGenerateWidget(this, &SAddNewGameplayTagSourceWidget::OnGenerateTagRootsComboBox)
				.ContentPadding(2.0f)
				.Content()
				[
					SNew(STextBlock)
					.Text(this, &SAddNewGameplayTagSourceWidget::CreateTagRootsComboBoxContent)
					.Font(IDetailLayoutBuilder::GetDetailFont())
				]
			]
		]

		// Add Source Button
		+SVerticalBox::Slot()
		.AutoHeight()
		.VAlign(VAlign_Top)
		.HAlign(HAlign_Center)
		.Padding(8.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOCTEXT("AddNew", "Add New Source"))
				.OnClicked(this, &SAddNewGameplayTagSourceWidget::OnAddNewSourceButtonPressed)
			]
		]
	];

	Reset();
}

void SAddNewGameplayTagSourceWidget::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if (bShouldGetKeyboardFocus)
	{
		bShouldGetKeyboardFocus = false;
		FSlateApplication::Get().SetKeyboardFocus(SourceNameTextBox.ToSharedRef(), EFocusCause::SetDirectly);
	}
}

void SAddNewGameplayTagSourceWidget::Reset()
{
	SetSourceName();
}

void SAddNewGameplayTagSourceWidget::SetSourceName(const FText& InName)
{
	SourceNameTextBox->SetText(InName.IsEmpty() ? FText::FromString(DefaultNewName) : InName);
}

void SAddNewGameplayTagSourceWidget::PopulateTagRoots()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	TagRoots.Empty();

	FName DefaultSource = FGameplayTagSource::GetDefaultName();

	TArray<FString> TagRootStrings;
	Manager.GetTagSourceSearchPaths(TagRootStrings);

	for (const FString& TagRoot : TagRootStrings)
	{
		TagRoots.Add(MakeShareable(new FString(TagRoot)));
	}
}

FText SAddNewGameplayTagSourceWidget::GetFriendlyPath(TSharedPtr<FString> InItem) const
{
	if (InItem.IsValid())
	{
		FString FilePath = *InItem.Get();

		if (FPaths::IsUnderDirectory(FilePath, FPaths::ProjectDir()))
		{
			FPaths::MakePathRelativeTo(FilePath, *FPaths::ProjectDir());
		}
		return FText::FromString(FilePath);
	}
	return FText();
}

TSharedRef<SWidget> SAddNewGameplayTagSourceWidget::OnGenerateTagRootsComboBox(TSharedPtr<FString> InItem)
{
	return SNew(STextBlock)
		.Text(GetFriendlyPath(InItem));
}

FText SAddNewGameplayTagSourceWidget::CreateTagRootsComboBoxContent() const
{
	const bool bHasSelectedItem = TagRootsComboBox.IsValid() && TagRootsComboBox->GetSelectedItem().IsValid();

	if (bHasSelectedItem)
	{
		return GetFriendlyPath(TagRootsComboBox->GetSelectedItem());
	}
	else
	{
		return LOCTEXT("NewTagRootNotSelected", "Default");
	}
}

FReply SAddNewGameplayTagSourceWidget::OnAddNewSourceButtonPressed()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
		
	if (!SourceNameTextBox->GetText().EqualTo(FText::FromString(DefaultNewName)))
	{
		FString TagRoot;
		if (TagRootsComboBox->GetSelectedItem().Get())
		{
			TagRoot = *TagRootsComboBox->GetSelectedItem().Get();
		}

		IGameplayTagsEditorModule::Get().AddNewGameplayTagSource(SourceNameTextBox->GetText().ToString(), TagRoot);
	}

	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
