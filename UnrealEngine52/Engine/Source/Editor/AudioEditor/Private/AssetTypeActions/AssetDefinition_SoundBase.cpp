// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetDefinition_SoundBase.h"
#include "AudioDeviceManager.h"
#include "ContentBrowserMenuContexts.h"
#include "Audio/AudioDebug.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "ToolMenus.h"
#include "Editor.h"
#include "Styling/AppStyle.h"
#include "Components/AudioComponent.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

namespace UE::AudioEditor
{
	void StopSound()
	{
		GEditor->ResetPreviewAudioComponent();
	}
	
	void PlaySound(USoundBase* Sound)
	{
		if ( Sound )
		{
			GEditor->PlayPreviewSound(Sound);
		}
		else
		{
			StopSound();
		}
	}
	
	bool IsSoundPlaying(USoundBase* Sound)
	{
		UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
		return PreviewComp && PreviewComp->Sound == Sound && PreviewComp->IsPlaying();
	}
	
	bool IsSoundPlaying(const FAssetData& AssetData)
	{
		const UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
		if (PreviewComp && PreviewComp->Sound && PreviewComp->IsPlaying())
		{
			if (PreviewComp->Sound->GetFName() == AssetData.AssetName)
			{
				if (PreviewComp->Sound->GetOutermost()->GetFName() == AssetData.PackageName)
				{
					return true;
				}
			}
		}
	
		return false;
	}
}

EAssetCommandResult UAssetDefinition_SoundBase::ActivateAssets(const FAssetActivateArgs& ActivateArgs) const
{
	if ( ActivateArgs.ActivationMethod == EAssetActivationMethod::Previewed )
	{
		if ( USoundBase* TargetSound = ActivateArgs.LoadFirstValid<USoundBase>() )
		{
			UAudioComponent* PreviewComp = GEditor->GetPreviewAudioComponent();
			if (PreviewComp && PreviewComp->IsPlaying())
			{
				// Already previewing a sound, if it is the target cue then stop it, otherwise play the new one
				if (!TargetSound || PreviewComp->Sound == TargetSound)
				{
					UE::AudioEditor::StopSound();
				}
				else
				{
					UE::AudioEditor::PlaySound(TargetSound);
				}
			}
			else
			{
				// Not already playing, play the target sound cue if it exists
				UE::AudioEditor::PlaySound(TargetSound);
			}
			
			return EAssetCommandResult::Handled;
		}
	}

	return Super::ActivateAssets(ActivateArgs);
}

TSharedPtr<SWidget> UAssetDefinition_SoundBase::GetThumbnailOverlay(const FAssetData& InAssetData) const
{
	auto OnGetDisplayBrushLambda = [this, InAssetData]() -> const FSlateBrush*
	{
		if (UE::AudioEditor::IsSoundPlaying(InAssetData))
		{
			return FAppStyle::GetBrush("MediaAsset.AssetActions.Stop.Large");
		}

		return FAppStyle::GetBrush("MediaAsset.AssetActions.Play.Large");
	};

	auto OnClickedLambda = [this, InAssetData]() -> FReply
	{
		if (UE::AudioEditor::IsSoundPlaying(InAssetData))
		{
			UE::AudioEditor::StopSound();
		}
		else
		{
			// Load and play sound
			UE::AudioEditor::PlaySound(Cast<USoundBase>(InAssetData.GetAsset()));
		}
		return FReply::Handled();
	};

	auto OnToolTipTextLambda = [this, InAssetData]() -> FText
	{
		if (UE::AudioEditor::IsSoundPlaying(InAssetData))
		{
			return LOCTEXT("Thumbnail_StopSoundToolTip", "Stop selected sound");
		}

		return LOCTEXT("Thumbnail_PlaySoundToolTip", "Play selected sound");
	};

	TSharedPtr<SBox> Box;
	SAssignNew(Box, SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2));

	auto OnGetVisibilityLambda = [this, Box, InAssetData]() -> EVisibility
	{
		if (Box.IsValid() && (Box->IsHovered() || UE::AudioEditor::IsSoundPlaying(InAssetData)))
		{
			return EVisibility::Visible;
		}

		return EVisibility::Hidden;
	};

	TSharedPtr<SButton> Widget;
	SAssignNew(Widget, SButton)
		.ButtonStyle(FAppStyle::Get(), "HoverHintOnly")
		.ToolTipText_Lambda(OnToolTipTextLambda)
		.Cursor(EMouseCursor::Default) // The outer widget can specify a DragHand cursor, so we need to override that here
		.ForegroundColor(FSlateColor::UseForeground())
		.IsFocusable(false)
		.OnClicked_Lambda(OnClickedLambda)
		.Visibility_Lambda(OnGetVisibilityLambda)
		[
			SNew(SImage)
			.Image_Lambda(OnGetDisplayBrushLambda)
		];

	Box->SetContent(Widget.ToSharedRef());
	Box->SetVisibility(EVisibility::Visible);

	return Box;
}

// Menu Extensions
//--------------------------------------------------------------------

namespace MenuExtension_SoundBase
{
	void ExecutePlaySound(const FToolMenuContext& InContext)
	{
		if (USoundBase* Sound = UContentBrowserAssetContextMenuContext::LoadSingleSelectedAsset<USoundBase>(InContext))
		{
			// Only play the first valid sound
			UE::AudioEditor::PlaySound(Sound);
		}
	}

	void ExecuteStopSound(const FToolMenuContext& InContext)
	{
		UE::AudioEditor::StopSound();
	}

	bool CanExecutePlayCommand(const FToolMenuContext& InContext)
	{
		if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
		{
			return CBContext->SelectedAssets.Num() == 1;
		}

		return false;
	}
	
	ECheckBoxState IsActionCheckedMute(const FToolMenuContext& InContext)
	{
#if ENABLE_AUDIO_DEBUG
		if (FAudioDeviceManager* ADM = GEditor->GetAudioDeviceManager())
		{
			if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
			{
				// If *any* of the selection are muted, show the tick box as ticked.
				Audio::FAudioDebugger& Debugger = ADM->GetDebugger();

				for (const FAssetData& SoundCueAsset : CBContext->GetSelectedAssetsOfType(USoundCue::StaticClass()))
				{
					if (Debugger.IsMuteSoundCue(SoundCueAsset.AssetName))
					{
						return ECheckBoxState::Checked;
					}
				}
				
				for (const FAssetData& SoundWaveAsset : CBContext->GetSelectedAssetsOfType(USoundWave::StaticClass()))
				{
					if (Debugger.IsMuteSoundWave(SoundWaveAsset.AssetName))
					{
						return ECheckBoxState::Checked;
					}
				}
			}
		}
#endif
		return ECheckBoxState::Unchecked;
	}
	
	ECheckBoxState IsActionCheckedSolo(const FToolMenuContext& InContext)
	{
#if ENABLE_AUDIO_DEBUG
		// If *any* of the selection are solod, show the tick box as ticked.
		if (FAudioDeviceManager* ADM = GEditor->GetAudioDeviceManager())
		{
			if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
			{
				Audio::FAudioDebugger& Debugger = ADM->GetDebugger();

				for (const FAssetData& SoundCueAsset : CBContext->GetSelectedAssetsOfType(USoundCue::StaticClass()))
				{
					if (Debugger.IsSoloSoundCue(SoundCueAsset.AssetName))
					{
						return ECheckBoxState::Checked;
					}
				}
				
				for (const FAssetData& SoundWaveAsset : CBContext->GetSelectedAssetsOfType(USoundWave::StaticClass()))
				{
					if (Debugger.IsSoloSoundWave(SoundWaveAsset.AssetName))
                    {
                    	return ECheckBoxState::Checked;
                    }
				}
			}
		}
#endif
		return ECheckBoxState::Unchecked;
	}

	void ExecuteMuteSound(const FToolMenuContext& InContext)
	{
#if ENABLE_AUDIO_DEBUG
		if (FAudioDeviceManager* ADM = GEditor->GetAudioDeviceManager())
		{
			if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
			{
				Audio::FAudioDebugger& Debugger = ADM->GetDebugger();

				// In a selection that consists of some already muted, toggle everything in the same direction,
				// to avoid AB problem.
				const bool bAnyMuted = IsActionCheckedMute(InContext) == ECheckBoxState::Checked;

				for (const FAssetData& SoundCueAsset : CBContext->GetSelectedAssetsOfType(USoundCue::StaticClass()))
				{
					Debugger.SetMuteSoundCue(SoundCueAsset.AssetName, !bAnyMuted);
				}
				
				for (const FAssetData& SoundWaveAsset : CBContext->GetSelectedAssetsOfType(USoundWave::StaticClass()))
				{
					Debugger.SetMuteSoundWave(SoundWaveAsset.AssetName, !bAnyMuted);
				}
			}
		}
#endif
	}

	void ExecuteSoloSound(const FToolMenuContext& InContext)
	{
#if ENABLE_AUDIO_DEBUG
		if (FAudioDeviceManager* ADM = GEditor->GetAudioDeviceManager())
		{
			if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
			{
				Audio::FAudioDebugger& Debugger = ADM->GetDebugger();

				// In a selection that consists of some already soloed, toggle everything in the same direction,
				// to avoid AB problem.

				const bool bAnySoloed = IsActionCheckedSolo(InContext) == ECheckBoxState::Checked;

				for (const FAssetData& SoundCueAsset : CBContext->GetSelectedAssetsOfType(USoundCue::StaticClass()))
				{
					Debugger.SetSoloSoundCue(SoundCueAsset.AssetName, !bAnySoloed);
				}
				
				for (const FAssetData& SoundWaveAsset : CBContext->GetSelectedAssetsOfType(USoundWave::StaticClass()))
				{
					Debugger.SetSoloSoundWave(SoundWaveAsset.AssetName, !bAnySoloed);
				}
			}
		}
#endif
	}	

	bool CanExecuteMuteCommand(const FToolMenuContext& InContext)
	{
#if ENABLE_AUDIO_DEBUG
		if (FAudioDeviceManager* ADM = GEditor->GetAudioDeviceManager())
		{
			if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
			{
				// Allow muting if we're not Soloing.
				Audio::FAudioDebugger& Debugger = ADM->GetDebugger();

				for (const FAssetData& SoundCueAsset : CBContext->GetSelectedAssetsOfType(USoundCue::StaticClass()))
				{
					if (Debugger.IsSoloSoundCue(SoundCueAsset.AssetName))
					{
						return false;
					}
				}
				
				for (const FAssetData& SoundWaveAsset : CBContext->GetSelectedAssetsOfType(USoundWave::StaticClass()))
				{
					if (Debugger.IsSoloSoundWave(SoundWaveAsset.AssetName))
					{
						return false;
					}
				}

				// Ok.
				return true;
			}
		}
#endif
		return false;
	}

	bool CanExecuteSoloCommand(const FToolMenuContext& InContext)
	{
#if ENABLE_AUDIO_DEBUG
		if (FAudioDeviceManager* ADM = GEditor->GetAudioDeviceManager())
		{
			if (const UContentBrowserAssetContextMenuContext* CBContext = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InContext))
			{
				// Allow Soloing if we're not Muting.
				Audio::FAudioDebugger& Debugger = ADM->GetDebugger();
			
				for (const FAssetData& SoundCueAsset : CBContext->GetSelectedAssetsOfType(USoundCue::StaticClass()))
				{
					if (Debugger.IsMuteSoundCue(SoundCueAsset.AssetName))
					{
						return false;
					}
				}
            
				for (const FAssetData& SoundWaveAsset : CBContext->GetSelectedAssetsOfType(USoundWave::StaticClass()))
				{
					if (Debugger.IsMuteSoundWave(SoundWaveAsset.AssetName))
					{
						return false;
					}
				}

				// Ok.
				return true;
			}
		}
#endif
		return false;
	}
	
	static FDelayedAutoRegisterHelper DelayedAutoRegister(EDelayedRegisterRunPhase::EndOfEngineInit, []{ 
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([]()
		{
			FToolMenuOwnerScoped OwnerScoped(UE_MODULE_NAME);
			UToolMenu* Menu = UE::ContentBrowser::ExtendToolMenu_AssetContextMenu(USoundBase::StaticClass());
		
			FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");
			Section.AddDynamicEntry(NAME_None, FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
			{
				if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection))
				{
					{
						const TAttribute<FText> Label = LOCTEXT("Sound_PlaySound", "Play");
						const TAttribute<FText> ToolTip = LOCTEXT("Sound_PlaySoundTooltip", "Plays the selected sound.");
						const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Play.Small");
						
						FToolUIAction UIAction;
						UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecutePlaySound);
						UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExecutePlayCommand);
						InSection.AddMenuEntry("Sound_PlaySound", Label, ToolTip, Icon, UIAction);
					}
					{
						const TAttribute<FText> Label = LOCTEXT("Sound_StopSound", "Stop");
						const TAttribute<FText> ToolTip = LOCTEXT("Sound_StopSoundTooltip", "Stops the selected sounds.");
						const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Stop.Small");
						
						FToolUIAction UIAction;
						UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteStopSound);
						InSection.AddMenuEntry("Sound_StopSound", Label, ToolTip, Icon, UIAction);
					}
					{
						const TAttribute<FText> Label = LOCTEXT("Sound_MuteSound", "Mute");
						const TAttribute<FText> ToolTip = LOCTEXT("Sound_MuteSoundTooltip", "Mutes the selected sounds.");
						const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Mute.Small");
		
						FToolUIAction UIAction;
						UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteMuteSound);
						UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExecuteMuteCommand);
						UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&IsActionCheckedMute);
						InSection.AddMenuEntry("Sound_SoundMute", Label, ToolTip, Icon, UIAction, EUserInterfaceActionType::ToggleButton);
					}
					{
						const TAttribute<FText> Label = LOCTEXT("Sound_SoloSound", "Solo");
						const TAttribute<FText> ToolTip = LOCTEXT("Sound_SoloSoundTooltip", "Solos the selected sounds.");
						const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Solo.Small");

						FToolUIAction UIAction;
						UIAction.ExecuteAction = FToolMenuExecuteAction::CreateStatic(&ExecuteSoloSound);
						UIAction.CanExecuteAction = FToolMenuCanExecuteAction::CreateStatic(&CanExecuteSoloCommand);
						UIAction.GetActionCheckState = FToolMenuGetActionCheckState::CreateStatic(&IsActionCheckedSolo);
						InSection.AddMenuEntry("Sound_StopSolo", Label, ToolTip, Icon, UIAction, EUserInterfaceActionType::ToggleButton);
					}
				}
			}));
		}));
	});
}

#undef LOCTEXT_NAMESPACE
