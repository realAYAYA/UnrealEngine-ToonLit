// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
template <typename OptionType> class SComboBox;
class SEditableTextBox;
class UDMXControlConsoleEditorGlobalLayoutBase;
class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	/** A widget to pick layouts for Control Console */
	class SDMXControlConsoleEditorLayoutPicker
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorLayoutPicker)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, UDMXControlConsoleEditorModel* InEditorModel);

	private:
		/** Generates a widget for selecting layouts */
		TSharedRef<SWidget> GenerateLayoutCheckBoxWidget();

		/** Generates a widget for Default Layout options */
		TSharedRef<SWidget> GenerateDefaultLayoutPickerWidget();

		/** Generates a widget for User Layout options */
		TSharedRef<SWidget> GenerateUserLayoutPickerWidget();

		/** Generates a widget for each element in the UserLayoutsComboBox */
		TSharedRef<SWidget> GenerateUserLayoutComboBoxWidget(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> InLayout);

		/** True if the active layout is the default layout */
		bool IsDefaultLayoutActive() const;

		/** Called when the Default Layout checkbox state has changed */
		void OnDefaultLayoutCheckBoxStateChanged(ECheckBoxState CheckBoxState);

		/** Called when the UserLayout checkbox state has changed */
		void OnUserLayoutCheckBoxStateChanged(ECheckBoxState CheckBoxState);

		/** Updates the ComboBoxSource array according to the current Control Console Layouts */
		void UpdateComboBoxSource();

		/** Gets the check box state of the auto-group check box */
		ECheckBoxState IsAutoGroupCheckBoxChecked() const;

		/** Called when the auto-group checkbox state has changed */
		void OnAutoGroupCheckBoxStateChanged(ECheckBoxState CheckBoxState);

		/** Called when a UserLayoutsComboBox element is selected */
		void OnComboBoxSelectionChanged(const TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> InLayout, ESelectInfo::Type SelectInfo);

		/** Called when the LayoutNameEditableBox text changes */
		void OnLayoutNameTextChanged(const FText& NewText);

		/** Called when a new text on the LayoutNameEditableBox editable text box is committed */
		void OnLayoutNameTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

		/** Called to rename the active layout */
		void OnRenameLayout(const FString& NewName);

		/** Called when the add layout button is clicked */
		FReply OnAddLayoutClicked();

		/** Called when the rename layout button is clicked */
		FReply OnRenameLayoutClicked();

		/** Called when the delete layout button is clicked */
		FReply OnDeleteLayoutClicked();

		/** Gets the visibility for the Default Layout option widgets */
		EVisibility GetDefaultLayoutVisibility() const;

		/** Gets the visibility for the User Layout option widgets */
		EVisibility GetUserLayoutVisibility() const;

		/** Reference to the last selected item in the UserLayoutsComboBox */
		TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase> LastSelectedItem;

		/** Source items for the UserLayoutsComboBox */
		TArray<TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase>> ComboBoxSource;

		/** A ComboBox for showing all User Layouts in the current Control Console */
		TSharedPtr<SComboBox<TWeakObjectPtr<UDMXControlConsoleEditorGlobalLayoutBase>>> UserLayoutsComboBox;

		/** Reference to the text box for editing the layout name */
		TSharedPtr<SEditableTextBox> LayoutNameEditableBox;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;

		/** Text for editing the layout name */
		FText LayoutNameText;
	};
}
