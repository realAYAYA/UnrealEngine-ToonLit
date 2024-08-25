// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FDMXEntityFixturePatchRef;
struct FSlateColor;
template <typename OptionType> class SComboBox;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroupController;
class UDMXEntityFixturePatch;
class UDMXLibrary;


namespace UE::DMX::Private
{
	class FDMXControlConsoleFaderGroupControllerModel;

	/** Combo box widget for selecting fixture patches in the Fader Group Controller view toolbar */
	class SDMXControlConsoleEditorFaderGroupControllerComboBox
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorFaderGroupControllerComboBox)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TWeakPtr<FDMXControlConsoleFaderGroupControllerModel>& InFaderGroupControllerModel, UDMXControlConsoleEditorModel* InEditorModel);

	private:
		/** Gets reference to the Fader Group Controller */
		UDMXControlConsoleFaderGroupController* GetFaderGroupController() const;

		/** Generates a widget for each element in the Fixture Patches Combo Box */
		TSharedRef<SWidget> GenerateFixturePatchesComboBoxWidget(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef);

		/** True if the given Fixture Patch is not used by any other Fader Group Controller */
		bool IsFixturePatchStillAvailable(const UDMXEntityFixturePatch* InFixturePatch) const;

		/** Updates the ComboBoxSource array according to the current DMX Library */
		void UpdateComboBoxSource();

		/** Called when a FixturePatchesComboBox element is selected */
		void OnComboBoxSelectionChanged(const TSharedPtr<FDMXEntityFixturePatchRef> FixturePatchRef, ESelectInfo::Type SelectInfo);

		/** Called when the text in the combo box text box is committed */
		void OnComboBoxTextCommitted(const FText& NewName, ETextCommit::Type InCommit);

		/** Gets the Fader Group Controller's editor color */
		FSlateColor GetFaderGroupControllerEditorColor() const;

		/** Gets the Fader Group Controller's fixture patch name as text, if valid */
		FText GetFaderGroupControllerFixturePatchNameText() const;

		/** Gets the Fader Group Controller user name as text */
		FText GetFaderGroupControllerUserNameText() const;

		/** True if the Fader Group Controller user name text box should be read only */
		bool IsUserNameTextBoxReadOnly() const;
		
		/** Gets the visibility for the Fader Group Controller user name text box */
		EVisibility GetUserNameTextBoxVisibility() const;

		/** Gets the visibility for the combo box image */
		EVisibility GetComboBoxVisibility() const;

		/** Reference to the current DMX Library */
		TWeakObjectPtr<UDMXLibrary> DMXLibrary;

		/** Source items for the FixturePatchesComboBox */
		TArray<TSharedPtr<FDMXEntityFixturePatchRef>> ComboBoxSource;

		/** A ComboBox for showing all active Fixture Patches in the current DMX Library */
		TSharedPtr<SComboBox<TSharedPtr<FDMXEntityFixturePatchRef>>> FixturePatchesComboBox;

		/** Weak Reference to the Fader Group Controller model */
		TWeakPtr<FDMXControlConsoleFaderGroupControllerModel> WeakFaderGroupControllerModel;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
