// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IPropertyTypeCustomization.h"
#include "PropertyHandle.h"

/**
 * Customizes Lighting Channels as a horizontal row of buttons.
 */
class DETAILCUSTOMIZATIONS_API FLightingChannelsCustomization : public IPropertyTypeCustomization
{
public:
	
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	FLightingChannelsCustomization(){}

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;

private:

	FText GetStructPropertyNameText() const;
	FText GetStructPropertyTooltipText() const;

	bool IsLightingChannelButtonEditable(uint32 ChildIndex) const;
	void OnButtonCheckedStateChanged(ECheckBoxState NewState, uint32 ChildIndex) const;
	ECheckBoxState GetButtonCheckedState(uint32 ChildIndex) const;

	TSharedPtr<IPropertyHandle> LightingChannelsHandle;
	FCheckBoxStyle Style;
};
