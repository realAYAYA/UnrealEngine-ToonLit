// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWaveformEditorInstantiator.h"
#include "Templates/SharedPointer.h"

class USoundWave;
struct FToolMenuContext;

class FWaveformEditorInstantiator : public IWaveformEditorInstantiator,  public TSharedFromThis<FWaveformEditorInstantiator>
{
public:
	virtual void ExtendContentBrowserSelectionMenu() override;
	virtual ~FWaveformEditorInstantiator() = default;

private:
	virtual void CreateWaveformEditor(TArray<USoundWave*> SoundWavesToEdit) override;
	void ExecuteCreateWaveformEditor(const FToolMenuContext& MenuContext);
	bool CanSoundWaveBeOpenedInEditor(const USoundWave* SoundWaveToEdit);
	void DisplayErrorDialog(const FText& ErrorMessage) const;
};