// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IWaveformEditorInstantiator.h"
#include "Templates/SharedPointer.h"

class FExtender;
class FMenuBuilder;
struct FAssetData;

class FWaveformEditorInstantiator : public IWaveformEditorInstantiator,  public TSharedFromThis<FWaveformEditorInstantiator>
{
public:
	virtual void ExtendContentBrowserSelectionMenu() override;
	virtual ~FWaveformEditorInstantiator() = default;

private:
	virtual void CreateWaveformEditor(TArray<USoundWave*> SoundWavesToEdit) override;

	TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
	void AddWaveformEditorMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets);
	bool CanSoundWaveBeOpenedInEditor(const USoundWave* SoundWaveToEdit);

	void DisplayErrorDialog(const FText& ErrorMessage) const;
};