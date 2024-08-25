// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class IPropertyUtilities;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleElementController;


namespace UE::DMX::Private
{
	/** Details Customization for DMX Control Console element controllers */
	class FDMXControlConsoleElementControllerDetails
		: public IDetailCustomization
	{
	public:
		/** Constructor */
		FDMXControlConsoleElementControllerDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		/** Makes an instance of this Details Customization */
		static TSharedRef<IDetailCustomization> MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		//~ Begin of IDetailCustomization interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;
		//~ End of IDetailCustomization interface

	private:
		/** Called when the Value property of the selected element controllers is changed */
		void OnSelectedElementControllersValueChanged() const;

		/** Called when the Min Value property of the selected element controllers is changed */
		void OnSelectedElementControllersMinValueChanged() const;

		/** Called when the Max Value property of the selected element controllers is changed */
		void OnSelectedElementControllersMaxValueChanged() const;

		/** Called when the Lock state of the selected element controllers is changed */
		void OnSelectedElementControllersLockStateChanged() const;

		/** Returns the element controllers being edited in these details. Only returns currently valid objects. */
		TArray<UDMXControlConsoleElementController*> GetValidElementControllersBeingEdited() const;

		/** Property Utilities for this Details Customization layout */
		TSharedPtr<IPropertyUtilities> PropertyUtilities;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
	};
}
