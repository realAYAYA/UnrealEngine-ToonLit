// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"

enum class EDMXFixtureSignalFormat : uint8;
class IPropertyUtilities;
class UDMXControlConsoleFaderBase;


namespace UE::DMXControlConsole
{
	/** Details Customization for DMX Control Console */
	class FDMXControlConsoleFaderDetails
		: public IDetailCustomization
	{
	public:
		/** Makes an instance of this Details Customization */
		static TSharedRef<IDetailCustomization> MakeInstance()
		{
			return MakeShared<FDMXControlConsoleFaderDetails>();
		}

		//~ Begin of IDetailCustomization interface
		virtual void CustomizeDetails(IDetailLayoutBuilder& InDetailLayout) override;
		//~ End of IDetailCustomization interface

	private:
		/** Forces refresh of the entire Details View */
		void ForceRefresh() const;

		/** True if all selected Faders are Raw Faders */
		bool HasOnlyRawFadersSelected() const;

		/** Called when selected faders Value property value is changed */
		void OnSelectedFadersValueChanged() const;

		/** Called when selected faders Min Value property value is changed */
		void OnSelectedFadersMinValueChanged() const;

		/** Called when selected faders Max Value property value is changed */
		void OnSelectedFadersMaxValueChanged() const;

		/** Returns the visibility of the MaxValue property's reset to default button */
		bool GetMaxValueResetToDefaultVisibility() const;

		/** Called when the MaxValue property's reset to default button was clicked */
		void OnMaxValueResetToDefaultClicked();

		/** Called when selected faders UniverseID property value is changed */
		void OnSelectedFadersUniverseIDChanged() const;

		/** Called when selected faders DataType property value is changed */
		void OnSelectedFadersDataTypeChanged() const;

		/** Returns the max value for specified signal format */
		uint32 GetMaxValueForSignalFormat(EDMXFixtureSignalFormat SignalFormat) const;

		/** Returns the faders being edited in these details. Only returns currently valid objects. */
		TArray<UDMXControlConsoleFaderBase*> GetValidFadersBeingEdited() const;

		/** Property Utilities for this Details Customization layout */
		TSharedPtr<IPropertyUtilities> PropertyUtilities;
	};

}
