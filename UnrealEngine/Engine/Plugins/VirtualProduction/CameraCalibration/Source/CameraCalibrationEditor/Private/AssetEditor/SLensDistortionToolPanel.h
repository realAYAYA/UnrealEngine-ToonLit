// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"

class FString;
class ULensFile;
class ULensDistortionTool;

template<typename OptionType>
class SComboBox;

struct FArguments;

/**
 * Wrapper UI for the specified calibration step.
 */
class SLensDistortionToolPanel : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLensDistortionToolPanel) {}
	SLATE_END_ARGS()

public:
	void Construct(const FArguments& InArgs, ULensDistortionTool* InTool);

private:

	/** Builds the wrapper for the currently selected algo UI */
	TSharedRef<SWidget> BuildUIWrapper();

	/** Builds the UI for the algorithm picker */
	TSharedRef<SWidget> BuildAlgoPickerWidget();

	/** Updates the UI so that it matches the selected algorithm (if necessary) */
	void UpdateUI();

	/** Refreshes the list of available algorithms shown in the AlgosComboBox */
	void UpdateAlgosOptions();

private:

	/** The tool controller object */
	TWeakObjectPtr<class ULensDistortionTool> Tool;

	/** The box containing the UI given by the selected algorithm */
	TSharedPtr<class SVerticalBox> UI;
	
private:

	/** Options source for the AlgosComboBox. Lists the currently available nodal offset algorithms */
	TArray<TSharedPtr<FString>> CurrentAlgos;

	/** The combobox that presents the available nodal offset algorithms */
	TSharedPtr<SComboBox<TSharedPtr<FString>>> AlgosComboBox;
};
