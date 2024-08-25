// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

enum class ECheckBoxState : uint8;
struct EVisibility;
class FReply;
class IPropertyUtilities;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroupController;


namespace UE::DMX::Private
{
	/** Details Customization for DMX Control Console fader group controllers */
	class FDMXControlConsoleFaderGroupControllerDetails
		: public IDetailCustomization
	{
	public:
		/** Constructor */
		FDMXControlConsoleFaderGroupControllerDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		/** Makes an instance of this Details Customization */
		static TSharedRef<IDetailCustomization> MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		//~ Begin of IDetailCustomization interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;
		//~ End of IDetailCustomization interface

	private:
		/** True if all the selected controllers have only unpatched fader groups */
		bool AreAllFaderGroupControllersUnpatched() const;

		/** True if the editor color of the selected controllers can be edited */
		bool IsFaderGroupControllersColorEditable() const;

		/** Called to toggle the lock state of the selected fader group controllers */
		void OnLockToggleChanged(ECheckBoxState CheckState);

		/** Gets the current lock state of the selected fader group controllers */
		ECheckBoxState IsLockChecked() const;

		/** Called when Clear button is clicked */
		FReply OnClearButtonClicked();

		/** Gets the visibility of the Editor Color Property */
		EVisibility GetEditorColorVisibility() const;

		/** Gets the visibility of the Clear button */
		EVisibility GetClearButtonVisibility() const;

		/** Returns the fader gorup controllers being edited in these details. Only returns currently valid objects. */
		TArray<UDMXControlConsoleFaderGroupController*> GetValidFaderGroupControllersBeingEdited() const;

		/** Property Utilities for this Details Customization layout */
		TSharedPtr<IPropertyUtilities> PropertyUtilities;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
	};
}
