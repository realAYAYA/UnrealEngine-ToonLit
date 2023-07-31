// Copyright Epic Games, Inc. All Rights Reserved.

#include "SDMXOutputConsole.h"

#include "DMXEditorStyle.h"
#include "DMXEditorLog.h"
#include "DMXEditorUtils.h"
#include "Widgets/OutputConsole/SDMXOutputFaderList.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Input/SButton.h"


#define LOCTEXT_NAMESPACE "SDMXOutputConsole"

void SDMXOutputConsole::Construct(const FArguments& InArgs)
{
	FadersAffectByMacrosSource.Add(MakeShared<FString>("Selected"));
	FadersAffectByMacrosSource.Add(MakeShared<FString>("All"));

	ChildSlot
	[
		SNew(SScrollBox)
		.Orientation(EOrientation::Orient_Vertical)
		+ SScrollBox::Slot()
		[
			// Fader List
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.VAlign(VAlign_Fill)
			.HAlign(HAlign_Fill)
			.AutoHeight()
			[
				SAssignNew(OutputFaderList, SDMXOutputFaderList)
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 4.0f, 0.0f, 4.0f))
			[
				SNew(SSeparator)
				.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
			]

			// Macros Section
			+ SVerticalBox::Slot()
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Top)
			.AutoHeight()
			[
				SNew(SVerticalBox)

				// Title
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Left)
				.MaxHeight(25.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.Padding(5.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT( "Macros", "Macros"))
						.MinDesiredWidth(100)
						.Justification(ETextJustify::Center)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 15))
					]
				]
							
				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 2.0f, 0.0f, 2.0f))
				[
					SNew(SSeparator)
					.ColorAndOpacity(FLinearColor(0.1f, 0.1f, 0.1f, 1.0f))
				]

				// Affected Faders section
				+ SVerticalBox::Slot()
				.VAlign(VAlign_Top)
				.HAlign(HAlign_Left)
				.MaxHeight(25.0f)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(12.f, 0.f)
					[
						SNew(STextBlock)
						.Text(LOCTEXT( "MacroAffectedFaders", "Affected Faders"))
						.MinDesiredWidth(100)
						.Justification(ETextJustify::Center)
						.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
					]
							
					+ SHorizontalBox::Slot()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Center)										
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					.AutoWidth()
					[
						SNew(SBox)
						.MinDesiredWidth(80.0f)
						[
							SAssignNew(FadersAffectedByMacroComboBox, SComboBox<TSharedPtr<FString>>)
							.OptionsSource(&FadersAffectByMacrosSource)
							.OnGenerateWidget(this, &SDMXOutputConsole::GenerateAffectedFadersComboBoxEntry)
							[
								SNew(STextBlock)
								.Text(this, &SDMXOutputConsole::GetFaderAffectByMacrosText)
							]
						]
					]
				]

				// Macro Buttons section
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					// Sine Wave Button
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.Padding(30.0f, 10.0f, 0.0f, 10.0f)	
					.AutoWidth()
					[
						GenerateMacroSineWaveButton()
					]

					// Max Button
					+ SHorizontalBox::Slot()
					.Padding(30.0f, 10.0f, 0.0f, 10.0f)	
					.AutoWidth()
					[
						GenerateMacroMaxButton()
					]

					// Min Button
					+ SHorizontalBox::Slot()
					.Padding(30.0f, 10.0f, 0.0f, 10.0f)					
					.AutoWidth()
					[
						GenerateMacroMinButton()
					]						
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0.0f, 4.0f, 0.0f, 4.0f))				
				[
					SNew(SSeparator)
					.ColorAndOpacity(FLinearColor(0.5f, 0.5f, 0.5f, 1.0f))
				]
			]
		]
	];
}

TSharedRef<SWidget> SDMXOutputConsole::GenerateMacroSineWaveButton()
{
	return
		SNew(SButton)
		.OnClicked(this, &SDMXOutputConsole::OnSineWaveMacroClicked)
		.ButtonColorAndOpacity(FColor(100, 100, 100, 255))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("SineWaveMacroLabel", "Sine Wave"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.Justification(ETextJustify::Center)
				.ColorAndOpacity(FLinearColor::White)
			]
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(4.0f, 8.0f, 4.0f, 8.0f))
			.AutoHeight()
			[
				SNew(SImage)
				.Image(FDMXEditorStyle::Get().GetBrush("DMXEditor.OutputConsole.MacroSineWave"))
			]
		];
}

TSharedRef<SWidget> SDMXOutputConsole::GenerateMacroMaxButton()
{
	return
		SNew(SButton)
		.OnClicked(this, &SDMXOutputConsole::OnMaxMacroClicked)
		.ButtonColorAndOpacity(FColor(100, 100, 100, 255))
		.ForegroundColor(FLinearColor::White)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MaxMacroLabel", "Max"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.Justification(ETextJustify::Center)
			]

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(4.0f, 8.0f, 4.0f, 8.0f))
			.AutoHeight()
			[
				SNew(SImage)
				.Image(FDMXEditorStyle::Get().GetBrush("DMXEditor.OutputConsole.MacroMax"))
			]
		];
}

TSharedRef<SWidget> SDMXOutputConsole::GenerateMacroMinButton()
{
	return		
		SNew(SButton)
		.OnClicked(this, &SDMXOutputConsole::OnMinMacroClicked)
		.ButtonColorAndOpacity(FColor(100, 100, 100, 255))
		.ForegroundColor(FLinearColor::White)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("MinMacroLabel", "Min"))
				.Font(FCoreStyle::GetDefaultFontStyle("Regular", 9))
				.Justification(ETextJustify::Center)
			]

			+ SVerticalBox::Slot()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.Padding(FMargin(4.0f, 8.0f, 4.0f, 8.0f))
			.AutoHeight()
			[
				SNew(SImage)
				.Image(FDMXEditorStyle::Get().GetBrush("DMXEditor.OutputConsole.MacroMin"))
			]
		];
}

TSharedRef<SWidget> SDMXOutputConsole::GenerateAffectedFadersComboBoxEntry(TSharedPtr<FString> Text)
{
	if (Text.IsValid())
	{
		return SNew(STextBlock)
			.Text(FText::FromString(*Text));
	}
	return SNew(STextBlock);
}

FText SDMXOutputConsole::GetFaderAffectByMacrosText() const
{
	TSharedPtr<FString> Selected = FadersAffectedByMacroComboBox->GetSelectedItem();
	if (!Selected.IsValid())
	{
		FadersAffectedByMacroComboBox->SetSelectedItem(FadersAffectByMacrosSource[0]);
		Selected = FadersAffectedByMacroComboBox->GetSelectedItem();
	}
	return FText::FromString(*Selected);
}

bool SDMXOutputConsole::ShouldMacrosAffectAllFaders() const
{
	TSharedPtr<FString> Selected = FadersAffectedByMacroComboBox->GetSelectedItem();
	return Selected == FadersAffectByMacrosSource[1];
}

FReply SDMXOutputConsole::OnSineWaveMacroClicked()
{
	check(OutputFaderList.IsValid());

	OutputFaderList->ApplySineWaveMacro(ShouldMacrosAffectAllFaders());
	return FReply::Handled();
}

FReply SDMXOutputConsole::OnMaxMacroClicked()
{
	check(OutputFaderList.IsValid());

	OutputFaderList->ApplyMaxValueMacro(ShouldMacrosAffectAllFaders());
	return FReply::Handled();
}

FReply SDMXOutputConsole::OnMinMacroClicked()
{
	check(OutputFaderList.IsValid());

	OutputFaderList->ApplyMinValueMacro(ShouldMacrosAffectAllFaders());
	return FReply::Handled();
}

#undef LOCTEXT_NAMESPACE
