// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateFwd.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/DeclarativeSyntaxSupport.h"

namespace UE::VCamCoreEditor::Private
{
	class FInputDeviceDetectionProcessor;
	
	DECLARE_DELEGATE_OneParam(FOnInputDeviceIDChanged, int32)

	/** Widgets for specifying device ID: has a button for listening for input from any device and numeric box for manual entry. */
	class SInputDeviceSelector : public SCompoundWidget
	{
	public:

		SLATE_BEGIN_ARGS(SInputDeviceSelector)
		{}
			/** That current input device value which is shown in the manual text edit box */
			SLATE_ATTRIBUTE(TOptional<int32>, CurrentInputDeviceID)
			/** Called when any input other than mouse input is received */
			SLATE_EVENT(FOnInputDeviceIDChanged, OnInputDeviceIDChanged)
		SLATE_END_ARGS()

		virtual ~SInputDeviceSelector() override;

		void Construct(const FArguments& InArgs);

	private:
		
		/** Called by ListenForInput when any input other than mouse input is received */
		FOnInputDeviceIDChanged OnInputDeviceIDChangedDelegate;
		/** Used by button to detect pressed input. */
		TSharedPtr<FInputDeviceDetectionProcessor> InputDeviceDetector;
		
		/** Text edit for manual user input */
		TSharedPtr<SNumericEntryBox<int32>> ManualEnterBox;

		/** Starts listening for input from any device other than mouse - blocks the input. */
		FReply ListenForInput();
		/** Util for destroying InputDeviceDetector */
		void StopListeningForInput();

		void OnAllowAnalogInputSettingToggled();
		
		/** Tints button orange while listening */
		FSlateColor GetKeyIconColor() const;

		/** Called when user commits value to ManualEnterBox. */
		void OnDeviceIDManuallyCommited(int32 Value, ETextCommit::Type CommitType);
	};
}
