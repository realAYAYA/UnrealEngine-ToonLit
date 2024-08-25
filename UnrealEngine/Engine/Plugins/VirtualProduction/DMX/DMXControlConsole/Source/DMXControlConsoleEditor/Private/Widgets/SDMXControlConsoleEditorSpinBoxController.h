// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Widgets/SCompoundWidget.h"

struct FSlateBrush;
template<typename NumericType> class SDMXControlConsoleEditorSpinBoxVertical;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleElementController;


namespace UE::DMX::Private
{
	class FDMXControlConsoleElementControllerModel;
	
	/** A widget to display a spin box which handles inputs for an Element Controller view */
	class SDMXControlConsoleEditorSpinBoxController
		: public SCompoundWidget
	{
	public:
		SLATE_BEGIN_ARGS(SDMXControlConsoleEditorSpinBoxController)
			{}

		SLATE_END_ARGS()

		/** Constructs the widget */
		void Construct(const FArguments& InArgs, const TSharedPtr<FDMXControlConsoleElementControllerModel>& InElementControllerModel, UDMXControlConsoleEditorModel* InEditorModel);

		/** Gets the element controller this widget is based on */
		UDMXControlConsoleElementController* GetElementController() const;

		/** Manually commits the given value */
		void CommitValue(float NewValue);

	protected:
		//~ Begin SWidget interface
		virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override;
		//~ End of SWidget interface

	private:
		/** True if the element controller is selected */
		bool IsSelected() const;

		/** Gets the element controller value */
		float GetValue() const;

		/** Returns the element controller value as text */
		FText GetValueAsText() const;

		/** Gets the element controller minimum value */
		TOptional<float> GetMinValue() const;

		/** Gets the element controller maximum value */
		TOptional<float> GetMaxValue() const;

		/** Handles when the user changes the element controller value */
		void HandleValueChanged(float NewValue);

		/** Called before the element controller value starts to change */
		void OnBeginValueChange();

		/** Called when a new element controller value is committed */
		void OnValueCommitted(float NewValue, ETextCommit::Type CommitType);

		/** Gets wheter the ElementControllerSpinBox widget should be active or not */
		bool IsElementControllerSpinBoxActive() const;

		/** Returns the element controller parameters as tooltip text */
		FText GetToolTipText() const;

		/** Changes the spin box background color on hover */
		const FSlateBrush* GetSpinBoxBorderImage() const;

		/** The actual editable element controller spin box */
		TSharedPtr<SDMXControlConsoleEditorSpinBoxVertical<float>> ElementControllerSpinBox;

		/** Reference to the element controller being displayed */
		TSharedPtr<FDMXControlConsoleElementControllerModel> ElementControllerModel;

		/** Weak reference to the control console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> EditorModel;

		/** The element controller value before committing */
		float PreCommittedValue = 0.f;
	};
}
