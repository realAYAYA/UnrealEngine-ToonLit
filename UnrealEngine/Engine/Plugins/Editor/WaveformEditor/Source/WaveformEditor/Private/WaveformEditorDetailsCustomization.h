// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"
#include "UObject/WeakObjectPtr.h"

class IDetailLayoutBuilder;

class FWaveformTransformationsDetailsCustomization : public IDetailCustomization
{
public:	
	void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

private:
	TWeakObjectPtr<UObject> SoundWaveObject;
};