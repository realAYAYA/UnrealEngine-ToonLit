// Copyright Epic Games, Inc. All Rights Reserved.

#include "AvaFontSearchSettingsMenu.h"
#include "Font/AvaFontManagerSubsystem.h"
#include "Font/AvaFontSelectorCommands.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "SAvaFontSelector.h"
#include "SlateOptMacros.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "AvaFontSearchSettingsMenu"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SAvaFontSearchSettingsMenu::Construct(const FArguments& InArgs)
{
	FontSelector = InArgs._AvaFontSelector;

	check(FontSelector.IsValid());

	UAvaFontManagerSubsystem* FontManagerSubsystem = UAvaFontManagerSubsystem::Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	if (const UAvaFontConfig* FontManagerConfig = FontManagerSubsystem->GetFontManagerConfig())
	{
		bIsShowMonospacedActive = FontManagerConfig->bShowOnlyMonospaced;
		bIsShowBoldActive = FontManagerConfig->bShowOnlyBold;
		bIsShowItalicActive = FontManagerConfig->bShowOnlyItalic;
	}

	BindCommands();

	ChildSlot
	[
		SNew(SComboButton)
		.CollapseMenuOnParentFocus(true)
		.ComboButtonStyle(&FAppStyle::Get().GetWidgetStyle<FComboButtonStyle>("SimpleComboButton"))
		.OnGetMenuContent(this, &SAvaFontSearchSettingsMenu::CreateSettingsMenu)
		.HasDownArrow(false)
		.ButtonContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.Padding(4.0f, 0.0f)
			[
				SNew(SImage)
				.ColorAndOpacity(FSlateColor::UseForeground())
				.Image(FAppStyle::Get().GetBrush("Icons.Settings"))
			]
		]
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> SAvaFontSearchSettingsMenu::CreateSettingsMenu() const
{
	const FAvaFontSelectorCommands& FontSelectorCommands = FAvaFontSelectorCommands::Get();

	constexpr bool bCloseAfterSelection = false;
	FMenuBuilder MenuBuilder(bCloseAfterSelection, CommandList);
	MenuBuilder.BeginSection(TEXT("FontsFilteringSettings"), LOCTEXT("FontsFiltering", "Fonts Filtering"));
	{
		MenuBuilder.AddMenuEntry(FontSelectorCommands.ShowMonospacedFonts);
		MenuBuilder.AddMenuEntry(FontSelectorCommands.ShowBoldFonts);
		MenuBuilder.AddMenuEntry(FontSelectorCommands.ShowItalicFonts);
	}
	MenuBuilder.EndSection();

	return MenuBuilder.MakeWidget();
}

void SAvaFontSearchSettingsMenu::BindCommands()
{
	FAvaFontSelectorCommands::Register();
	const FAvaFontSelectorCommands& FontSelectorCommands = FAvaFontSelectorCommands::Get();

	CommandList = MakeShared<FUICommandList>();

	// Monospaced filter
	CommandList->MapAction(FontSelectorCommands.ShowMonospacedFonts
		, FExecuteAction::CreateSP(this, &SAvaFontSearchSettingsMenu::ShowMonospacedToggle_Execute)
		, FCanExecuteAction::CreateSP(this, &SAvaFontSearchSettingsMenu::ShowMonospacedToggle_CanExecute)
		, FIsActionChecked::CreateSP(this, &SAvaFontSearchSettingsMenu::ShowMonospacedToggle_IsChecked));

	// Bold filter
	CommandList->MapAction(FontSelectorCommands.ShowBoldFonts
		, FExecuteAction::CreateSP(this, &SAvaFontSearchSettingsMenu::ShowBoldToggle_Execute)
		, FCanExecuteAction::CreateSP(this, &SAvaFontSearchSettingsMenu::ShowBoldToggle_CanExecute)
		, FIsActionChecked::CreateSP(this, &SAvaFontSearchSettingsMenu::ShowBoldToggle_IsChecked));

	// Italic filter
	CommandList->MapAction(FontSelectorCommands.ShowItalicFonts
		, FExecuteAction::CreateSP(this, &SAvaFontSearchSettingsMenu::ShowItalicToggle_Execute)
		, FCanExecuteAction::CreateSP(this, &SAvaFontSearchSettingsMenu::ShowItalicToggle_CanExecute)
		, FIsActionChecked::CreateSP(this, &SAvaFontSearchSettingsMenu::ShowItalicToggle_IsChecked));
}

void SAvaFontSearchSettingsMenu::ShowMonospacedToggle_Execute()
{
	UAvaFontManagerSubsystem* FontManagerSubsystem = UAvaFontManagerSubsystem::Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	bIsShowMonospacedActive = !bIsShowMonospacedActive;
	FontManagerSubsystem->GetFontManagerConfig()->ToggleShowMonospaced();

	if (FontSelector)
	{
		FontSelector->SetShowMonospacedOnly(bIsShowMonospacedActive);
	}
}

void SAvaFontSearchSettingsMenu::ShowBoldToggle_Execute()
{
	UAvaFontManagerSubsystem* FontManagerSubsystem = UAvaFontManagerSubsystem::Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	bIsShowBoldActive = !bIsShowBoldActive;
	FontManagerSubsystem->GetFontManagerConfig()->ToggleShowBold();

	if (FontSelector)
	{
		FontSelector->SetShowBoldOnly(bIsShowBoldActive);
	}
}

void SAvaFontSearchSettingsMenu::ShowItalicToggle_Execute()
{
	UAvaFontManagerSubsystem* FontManagerSubsystem = UAvaFontManagerSubsystem::Get();
	if (!FontManagerSubsystem)
	{
		return;
	}

	bIsShowItalicActive = !bIsShowItalicActive;
	FontManagerSubsystem->GetFontManagerConfig()->ToggleShowItalic();

	if (FontSelector)
	{
		FontSelector->SetShowItalicOnly(bIsShowItalicActive);
	}
}

#undef LOCTEXT_NAMESPACE
