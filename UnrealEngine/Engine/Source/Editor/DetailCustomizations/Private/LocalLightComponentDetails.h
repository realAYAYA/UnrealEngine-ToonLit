// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class ULocalLightComponent;

class FLocalLightComponentDetails : public IDetailCustomization
{
public:
	FLocalLightComponentDetails() : LastLightBrigtness(0) {}

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;
	virtual void CustomizeDetails( const TSharedPtr<IDetailLayoutBuilder>& DetailBuilder ) override;

protected:

	// The detail builder for this cusomtomisation
	TWeakPtr<IDetailLayoutBuilder> CachedDetailBuilder;

	void ResetIntensityUnitsToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, ULocalLightComponent* Component);
	bool IsIntensityUnitsResetToDefaultVisible(TSharedPtr<IPropertyHandle> PropertyHandle, ULocalLightComponent* Component) const;


	/** Called when the intensity units are changed */
	void OnIntensityUnitsPreChange(ULocalLightComponent* Component);
	void OnIntensityUnitsChanged(ULocalLightComponent* Component);

	float LastLightBrigtness;
};
