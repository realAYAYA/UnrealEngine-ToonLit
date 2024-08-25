// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class IPropertyUtilities;
class UDMXControlConsoleEditorModel;
class UDMXControlConsoleFaderGroup;


namespace UE::DMX::Private
{
	/** Details Customization for DMX Control Console fader groups */
	class FDMXControlConsoleFaderGroupDetails
		: public IDetailCustomization
	{
	public:
		/** Constructor */
		FDMXControlConsoleFaderGroupDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		/** Makes an instance of this Details Customization */
		static TSharedRef<IDetailCustomization> MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		//~ Begin of IDetailCustomization interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;
		//~ End of IDetailCustomization interface

	private:
		/** True if at least one selected fader group has any fixture patch bound */
		bool IsAnyFaderGroupPatched() const;

		/** Gets the name of the current selected fader group's Fixture Patch */
		FText GetFixturePatchText() const;

		/** Returns the fader groups being edited in these details. Only returns currently valid objects. */
		TArray<UDMXControlConsoleFaderGroup*> GetValidFaderGroupsBeingEdited() const;

		/** Property Utilities for this Details Customization layout */
		TSharedPtr<IPropertyUtilities> PropertyUtilities;

		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
	};
}
