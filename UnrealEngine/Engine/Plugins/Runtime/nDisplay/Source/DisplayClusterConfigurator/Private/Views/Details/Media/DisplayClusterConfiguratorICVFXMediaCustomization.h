// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Views/Details/DisplayClusterConfiguratorBaseTypeCustomization.h"

#include "Input/Reply.h"

#include "IPropertyTypeCustomization.h"


/**
 * Details panel customization for the FDisplayClusterConfigurationMediaICVFX struct.
 */
class FDisplayClusterConfiguratorICVFXMediaCustomization
	: public FDisplayClusterConfiguratorBaseTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShared<FDisplayClusterConfiguratorICVFXMediaCustomization>();
	}

protected:
	//~ Begin IPropertyTypeCustomization
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	//~ End IPropertyTypeCustomization

private:

	/** Builds auto-conf button widget. */
	void AddAutoConfigurationButton(IDetailChildrenBuilder& ChildBuilder);

	/** Handles auto-configuration button clicks. */
	FReply OnAutoConfigureButtonClicked();

private:

	/** Tiles layout property. */
	TSharedPtr<IPropertyHandle> TilesLayoutHandle;
};
