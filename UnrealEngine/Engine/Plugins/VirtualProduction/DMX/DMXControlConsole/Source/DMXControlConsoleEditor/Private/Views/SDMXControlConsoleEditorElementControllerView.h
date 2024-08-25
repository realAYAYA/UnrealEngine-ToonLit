// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

enum class ECheckBoxState : uint8;
struct FOptionalSize;
struct FSlateColor;
class SButton;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleElementController;


namespace UE::DMX::Private
{
	class FDMXControlConsoleElementControllerModel;
	class SDMXControlConsoleEditorSpinBoxController;

	/** Individual element controller UI class */
	class SDMXControlConsoleEditorElementControllerView
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorElementControllerView)
			{}

			SLATE_ARGUMENT(FMargin, Padding)

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleElementControllerModel>& InElementControllerModel, UDMXControlConsoleEditorModel* InEditorModel);

		/** Gets the element controller this view is based on */
		UDMXControlConsoleElementController* GetElementController() const;

	protected:
		//~ Begin SWidget interface
		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		//~ End of SWidget interface

	private:
		/** Generates the lock button widget  */
		TSharedRef<SWidget> GenerateLockButtonWidget();

		/** Generates a context menu widget for element controller options  */
		TSharedRef<SWidget> GenerateElementControllerContextMenuWidget();

		/** True if the element controller is selected */
		bool IsSelected() const;

		/** Gets the element controller name */
		FString GetElementControllerName() const;

		/**  Gets the current element controller name as text */
		FText GetElementControllerNameText() const;

		/** Returns the element controller value as text */
		FText GetValueAsText() const;

		/** Called when a new text on value editable text box is committed */
		void OnValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

		/** Gets the element controller minimum value */
		TOptional<float> GetMinValue() const;

		/** Returns the element controller minimum value as text */
		FText GetMinValueAsText() const;

		/** Called when a new text on minimum value editable text box is committed */
		void OnMinValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

		/** Gets the element controller maximum value */
		TOptional<float> GetMaxValue() const;

		/** Returns the element controller maximum value as text */
		FText GetMaxValueAsText() const;

		/** Called when a new text on maximum value editable text box is committed */
		void OnMaxValueTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

		/** Called when the enable option is selected */
		void OnEnableElementController(bool bEnable) const;

		/** Called when the remove option is selected */
		void OnRemoveElementController() const;

		/** Called when the reset option is selected */
		void OnResetElementController() const;

		/** Called when the lock option is selected */
		void OnLockElementController(bool bLock) const;

		/** Called to lock/unlock this element controller */
		FReply OnLockClicked();

		/** Checks the current enable state of the element controller */
		ECheckBoxState IsEnableChecked() const;

		/** Called to toggle the enable state of this element controller */
		void OnEnableToggleChanged(ECheckBoxState CheckState);

		/** Gets the height of the element controller according to the current view mode  */
		FOptionalSize GetElementControllerHeightByViewMode() const;

		/** Gets the correct text for the lock button */
		FSlateColor GetLockButtonColor() const;

		/** Gets the visibility for the toolbar sections only visible in expanded view mode */
		EVisibility GetExpandedViewModeVisibility() const;

		/** Gets the visibility for the lock button  */
		EVisibility GetLockButtonVisibility() const;

		/** Changes the element controller background color on hover */
		const FSlateBrush* GetBorderImage() const;

		/** Reference to the lock button widget */
		TSharedPtr<SButton> LockButton;

		/** Reference to the spin box controller widget */
		TSharedPtr<SDMXControlConsoleEditorSpinBoxController> SpinBoxControllerWidget;

		/** Reference to the element controller being displayed */
		TSharedPtr<FDMXControlConsoleElementControllerModel> ElementControllerModel;

		/** Weak reference to the control console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;
	};
}
