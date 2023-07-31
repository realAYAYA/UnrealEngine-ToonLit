// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "Input/Reply.h"
#include "Templates/SharedPointer.h"

class ALandscapeProxy;
class IDetailLayoutBuilder;

class FLandscapeProxyUIDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;
	/** End IDetailCustomization interface */

private:
	/** Use MakeInstance to create an instance of this class */
	FLandscapeProxyUIDetails();

		/** Returns true if SetBounds button is enabled */
	bool IsCreateRuntimeVirtualTextureVolumeEnabled() const;
	/** Callback for Set Bounds button */
	FReply CreateRuntimeVirtualTextureVolume();
	
	IDetailLayoutBuilder* DetailLayoutBuilder = nullptr;
	ALandscapeProxy* LandscapeProxy = nullptr;
};
