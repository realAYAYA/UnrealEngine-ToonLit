// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "Styling/SlateTypes.h"

class IDetailLayoutBuilder;
class AWorldDataLayers;

class FWorldDataLayersActorDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	bool UseExternalPackageDataLayerInstancesEnabled(AWorldDataLayers* WorldDataLayers) const;
	ECheckBoxState IsUseExternalPackageDataLayerInstancesChecked(AWorldDataLayers* WorldDataLayers) const;
	void OnUseExternalPackageDataLayerInstancesChanged(ECheckBoxState BoxState, AWorldDataLayers* WorldDataLayers);
};
