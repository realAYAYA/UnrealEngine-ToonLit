// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Input/SComboBox.h"

class FDMXEditor;
class SDMXOutputFaderList;

/**
 * Widget for the Output Console tab, to configure output faders 
 */
class SDMXOutputConsole
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SDMXOutputConsole)
	{}
		
	SLATE_END_ARGS()

	/** Constructs the widget */
	void Construct(const FArguments& InArgs);

private:
	/** Generates an entry in the 'affected faders' ComboBox */
	TSharedRef<SWidget> GenerateAffectedFadersComboBoxEntry(TSharedPtr<FString> Text);

	/** Generates the 'sine wave macro' button */
	TSharedRef<SWidget> GenerateMacroSineWaveButton();

	/** Generates the 'max macro' button */
	TSharedRef<SWidget> GenerateMacroMaxButton();

	/** Generates the 'min macro' button */
	TSharedRef<SWidget> GenerateMacroMinButton();

	/** Returns which faders are affected by macros, as text */
	FText GetFaderAffectByMacrosText() const;

	/** Returns true if macros should affect all faders */
	bool ShouldMacrosAffectAllFaders() const;

	/** Called when the sine wave macro button was clicked */
	FReply OnSineWaveMacroClicked();

	/** Called when the max macro button was clicked */
	FReply OnMaxMacroClicked();

	/** Called when the min macro button was clicked */
	FReply OnMinMacroClicked();

	/** The ComboBox to select which Faders should be affected by Macros */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> FadersAffectedByMacroComboBox;
	
	/** Source for the 'affected faders' ComboBox */
	TArray<TSharedPtr<FString>> FadersAffectByMacrosSource;

	/** The fader list widget */
	TSharedPtr<SDMXOutputFaderList> OutputFaderList;
};
