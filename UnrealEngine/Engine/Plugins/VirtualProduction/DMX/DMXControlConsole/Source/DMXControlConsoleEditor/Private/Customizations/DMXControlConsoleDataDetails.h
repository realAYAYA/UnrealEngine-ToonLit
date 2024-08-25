// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class UDMXControlConsoleEditorModel;


namespace UE::DMX::Private
{
	/** Details Customization for DMX Control Console */
	class FDMXControlConsoleDataDetails
		: public IDetailCustomization
	{
	public:
		/** Constructor */
		FDMXControlConsoleDataDetails(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

		/** Makes an instance of this Details Customization */
		static TSharedRef<IDetailCustomization> MakeInstance(const TWeakObjectPtr<UDMXControlConsoleEditorModel> InWeakEditorModel);

	protected:
		//~ Begin of IDetailCustomization interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;
		//~ End of IDetailCustomization interface

	private:
		/** Weak reference to the Control Console editor model */
		TWeakObjectPtr<UDMXControlConsoleEditorModel> WeakEditorModel;
	};
}
