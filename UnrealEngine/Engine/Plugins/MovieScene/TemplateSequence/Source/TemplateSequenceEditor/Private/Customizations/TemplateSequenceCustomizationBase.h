// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ISequencer.h"
#include "SequencerCustomizationManager.h"

class UTemplateSequence;

/**
 * Base implementation of a template sequence customization for Sequencer.
 */
class FTemplateSequenceCustomizationBase : public ISequencerCustomization
{
protected:
	virtual void RegisterSequencerCustomization(FSequencerCustomizationBuilder& Builder) override;
	virtual void UnregisterSequencerCustomization() override;

	UClass* GetBoundActorClass() const;
	FText GetBoundActorClassName() const;
	void OnBoundActorClassPicked(UClass* ChosenClass);
	void ChangeActorBinding(UObject* Object, UActorFactory* ActorFactory = nullptr, bool bSetupDefaults = true);

	ISequencer* GetSequencer() const { return Sequencer; }
	UTemplateSequence* GetTemplateSequence() const { return TemplateSequence; }

private:
	ESequencerPasteSupport OnPaste();
	void OnMovieSceneDataChanged(EMovieSceneDataChangeType ChangeType);

	ISequencer* Sequencer;
	UTemplateSequence* TemplateSequence;
};

