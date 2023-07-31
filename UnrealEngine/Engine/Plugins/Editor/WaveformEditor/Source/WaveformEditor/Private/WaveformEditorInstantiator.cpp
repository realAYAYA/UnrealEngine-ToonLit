// Copyright Epic Games, Inc. All Rights Reserved.

#include "WaveformEditorInstantiator.h"

#include "ContentBrowserModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Sound/SoundWave.h"
#include "SWaveformEditorMessageDialog.h"
#include "WaveformEditor.h"
#include "WaveformEditorLog.h"

#define LOCTEXT_NAMESPACE "WaveformEditorInstantiator"

void FWaveformEditorInstantiator::ExtendContentBrowserSelectionMenu()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::GetModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));

	TArray<FContentBrowserMenuExtender_SelectedAssets>& ContentBrowserExtenders = ContentBrowserModule.GetAllAssetViewContextMenuExtenders();
	ContentBrowserExtenders.Add(FContentBrowserMenuExtender_SelectedAssets::CreateSP(this, &FWaveformEditorInstantiator::OnExtendContentBrowserAssetSelectionMenu));
}

TSharedRef<FExtender> FWaveformEditorInstantiator::OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender = MakeShared<FExtender>();

	Extender->AddMenuExtension(
		"GetAssetActions",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateSP(this, &FWaveformEditorInstantiator::AddWaveformEditorMenuEntry, SelectedAssets)
	);

	return Extender;
}

void FWaveformEditorInstantiator::AddWaveformEditorMenuEntry(FMenuBuilder& MenuBuilder, TArray<FAssetData> SelectedAssets)
{
	if (SelectedAssets.Num() > 0)
	{
		// check that all selected assets are USoundWaves.
		TArray<USoundWave*> SelectedSoundWaves;
		for (const FAssetData& SelectedAsset : SelectedAssets)
		{
			if (SelectedAsset.GetClass() != USoundWave::StaticClass())
			{
				return;
			}

			SelectedSoundWaves.Add(static_cast<USoundWave*>(SelectedAsset.GetAsset()));
		}

		MenuBuilder.AddMenuEntry(
			LOCTEXT("SoundWave_WaveformEditor", "Edit Waveform"),
			LOCTEXT("SoundWave_WaveformEditor_Tooltip", "Open waveform editor"),
			FSlateIcon(),
			FUIAction(
				FExecuteAction::CreateSP(this, &FWaveformEditorInstantiator::CreateWaveformEditor, SelectedSoundWaves),
				FCanExecuteAction()
			)
		);
	}
}

bool FWaveformEditorInstantiator::CanSoundWaveBeOpenedInEditor(const USoundWave* SoundWaveToEdit)
{
	bool bCanOpenWaveEditor = true;
	
	FText ErrorText = LOCTEXT("WaveformEditorOpeningError", "Could not open waveform editor for Selected SoundWave");

	if (SoundWaveToEdit == nullptr)
	{
		ErrorText = LOCTEXT("WaveformEditorOpeningError_NullSoundWave", "Could not open waveform editor. Selected SoundWave was null.");
		bCanOpenWaveEditor = false;
	}
	else
	{
		FText SoundWaveNameText = FText::FromString(*(SoundWaveToEdit->GetName()));
		
		if (SoundWaveToEdit->GetDuration() == 0.f)
		{
			ErrorText = FText::Format(LOCTEXT("WaveformEditorOpeningError_ZeroDuration", "Could not open waveform editor for SoundWave '{0}': duration is 0"), SoundWaveNameText);
			bCanOpenWaveEditor = false;
		}

		if (SoundWaveToEdit->NumChannels == 0)
		{
			ErrorText = FText::Format(LOCTEXT("WaveformEditorOpeningError_ZeroChannels", "Could not open waveform editor for SoundWave '{0}': channel count is 0"), SoundWaveNameText);
			bCanOpenWaveEditor = false;
		}

		if (SoundWaveToEdit->TotalSamples == 0)
		{
			ErrorText = FText::Format(LOCTEXT("WaveformEditorOpeningError_ZeroSamples", "Could not open waveform editor for SoundWave '{0}': found 0 total samples.\n\nConsider reimporting the asset to fix it."), SoundWaveNameText);
			bCanOpenWaveEditor = false;
		}
	}


	if (!bCanOpenWaveEditor)
	{
		DisplayErrorDialog(ErrorText);
	}
	
	return bCanOpenWaveEditor;
}

void FWaveformEditorInstantiator::DisplayErrorDialog(const FText& ErrorMessage) const
{
	UE_LOG(LogWaveformEditor, Warning, TEXT("%s"), *ErrorMessage.ToString())

	TSharedPtr<SWindow> OpeningErrorWindow = SNew(SWindow)
		.Title(LOCTEXT("WaveEditorErrorWindowTitle", "Waveform Editor"))
		.HasCloseButton(true)
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::Autosized);

	OpeningErrorWindow->SetContent(SNew(SWaveformEditorMessageDialog).ParentWindow(OpeningErrorWindow).MessageToDisplay(ErrorMessage));

	TSharedPtr<SWindow> RootWindow = FGlobalTabmanager::Get()->GetRootWindow();

	if (RootWindow.IsValid())
	{
		FSlateApplication::Get().AddModalWindow(OpeningErrorWindow.ToSharedRef(), RootWindow);
	}
	else
	{
		FSlateApplication::Get().AddWindow(OpeningErrorWindow.ToSharedRef());
	}
}

void FWaveformEditorInstantiator::CreateWaveformEditor(TArray<USoundWave*> SoundWavesToEdit)
{
	for (USoundWave* SoundWavePtr : SoundWavesToEdit)
	{
		if (CanSoundWaveBeOpenedInEditor(SoundWavePtr))
		{
			TSharedRef<FWaveformEditor> WaveformEditor = MakeShared<FWaveformEditor>();

			if (!WaveformEditor->Init(EToolkitMode::Standalone, nullptr, SoundWavePtr))
			{
				UE_LOG(LogWaveformEditor, Warning, TEXT("Could not open waveform editor for soundwave %s, initialization failed"), *(SoundWavePtr->GetName()))
				WaveformEditor->CloseWindow();
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE