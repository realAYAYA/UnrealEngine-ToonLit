// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TemplateSequenceCustomizationBase.h"

class FToolBarBuilder;

/**
 * The sequencer customization for camera animation sequences.
 */
class FCameraAnimationSequenceCustomization : public FTemplateSequenceCustomizationBase
{
private:
	virtual void RegisterSequencerCustomization(FSequencerCustomizationBuilder& Builder) override;
	virtual void UnregisterSequencerCustomization() override;

	void ExtendSequencerToolbar(FToolBarBuilder& ToolbarBuilder);

	TSharedRef<SWidget> GetBoundCameraClassMenuContent();
	bool IsBoundToActorClass(UClass* InClass);

	TArray<UClass*> CameraActorClasses;
};

