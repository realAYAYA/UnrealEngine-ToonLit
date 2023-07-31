// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_DialogueWave.h"

#include "AssetToolsModule.h"
#include "Containers/UnrealString.h"
#include "ContentBrowserModule.h"
#include "Delegates/Delegate.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Framework/Commands/UIAction.h"
#include "HAL/PlatformCrt.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundCue.h"
#include "Styling/AppStyle.h"
#include "Templates/Casts.h"
#include "Textures/SlateIcon.h"
#include "ToolMenuSection.h"
#include "Toolkits/IToolkit.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "UObject/Object.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

#include <utility>

class IToolkitHost;
class USoundBase;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

void FAssetTypeActions_DialogueWave::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	auto DialogueWaves = GetTypedWeakObjectPtrs<UDialogueWave>(InObjects);

	Section.AddMenuEntry(
		"Sound_PlaySound",
		LOCTEXT("Sound_PlaySound", "Play"),
		LOCTEXT("Sound_PlaySoundTooltip", "Plays the selected sound."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Play.Small"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::ExecutePlaySound, DialogueWaves),
			FCanExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::CanExecutePlayCommand, DialogueWaves)
		)
	);

	Section.AddMenuEntry(
		"Sound_StopSound",
		LOCTEXT("Sound_StopSound", "Stop"),
		LOCTEXT("Sound_StopSoundTooltip", "Stops the selected sounds."),
		FSlateIcon(FAppStyle::GetAppStyleSetName(), "MediaAsset.AssetActions.Stop.Small"),
		FUIAction(
			FExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::ExecuteStopSound, DialogueWaves),
			FCanExecuteAction()
		)
	);

	bool bCreateCueForEachDialogueWave = true;
	if (DialogueWaves.Num() == 1)
	{
		Section.AddMenuEntry(
			"DialogueWave_CreateCue",
			LOCTEXT("DialogueWave_CreateCue", "Create Cue"),
			LOCTEXT("DialogueWave_CreateCueTooltip", "Creates a sound cue using this dialogue wave."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SoundCue"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::ExecuteCreateSoundCue, DialogueWaves, bCreateCueForEachDialogueWave),
				FCanExecuteAction()
			)
		);
	}
	else
	{
		bCreateCueForEachDialogueWave = false;
		Section.AddMenuEntry(
			"DialogueWave_CreateSingleCue",
			LOCTEXT("DialogueWave_CreateSingleCue", "Create Single Cue"),
			LOCTEXT("DialogueWave_CreateSingleCueTooltip", "Creates a single sound cue using these dialogue waves."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SoundCue"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::ExecuteCreateSoundCue, DialogueWaves, bCreateCueForEachDialogueWave),
				FCanExecuteAction()
			)
		);

		bCreateCueForEachDialogueWave = true;
		Section.AddMenuEntry(
			"DialogueWave_CreateMultipleCue",
			LOCTEXT("DialogueWave_CreateMultipleCue", "Create Multiple Cues"),
			LOCTEXT("DialogueWave_CreateMultipleCueTooltip", "Creates multiple sound cues, one from each dialogue wave."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SoundCue"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_DialogueWave::ExecuteCreateSoundCue, DialogueWaves, bCreateCueForEachDialogueWave),
				FCanExecuteAction()
			)
		);
	}

}

void FAssetTypeActions_DialogueWave::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	EToolkitMode::Type Mode = EditWithinLevelEditor.IsValid() ? EToolkitMode::WorldCentric : EToolkitMode::Standalone;

	for (auto ObjIt = InObjects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		auto DialogueWave = Cast<UDialogueWave>(*ObjIt);
		if (DialogueWave != NULL)
		{
			FSimpleAssetEditor::CreateEditor(Mode, Mode == EToolkitMode::WorldCentric ? EditWithinLevelEditor : TSharedPtr<IToolkitHost>(), DialogueWave);
		}
	}
}

bool FAssetTypeActions_DialogueWave::CanExecutePlayCommand(TArray<TWeakObjectPtr<UDialogueWave>> Objects) const
{
	if (Objects.Num() != 1)
	{
		return false;
	}

	USoundBase* Sound = nullptr;

	auto DialogueWave = Objects[0].Get();
	for (int32 i = 0; i < DialogueWave->ContextMappings.Num(); ++i)
	{
		const FDialogueContextMapping& ContextMapping = DialogueWave->ContextMappings[i];

		Sound = DialogueWave->GetWaveFromContext(ContextMapping.Context);
		if (Sound != nullptr)
		{
			break;
		}
	}

	return Sound != nullptr;
}

void FAssetTypeActions_DialogueWave::ExecutePlaySound(TArray<TWeakObjectPtr<UDialogueWave>> Objects)
{
	for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
	{
		UDialogueWave* DialogueWave = (*ObjIt).Get();
		if (DialogueWave)
		{
			// Only play the first valid sound
			PlaySound(DialogueWave);
			break;
		}
	}
}

void FAssetTypeActions_DialogueWave::ExecuteStopSound(TArray<TWeakObjectPtr<UDialogueWave>> Objects)
{
	StopSound();
}

void FAssetTypeActions_DialogueWave::PlaySound(UDialogueWave* DialogueWave)
{
	USoundBase* Sound = nullptr;

	for (int32 i = 0; i < DialogueWave->ContextMappings.Num(); ++i)
	{
		const FDialogueContextMapping& ContextMapping = DialogueWave->ContextMappings[i];

		Sound = DialogueWave->GetWaveFromContext(ContextMapping.Context);
		if (Sound != nullptr)
		{
			break;
		}
	}

	if (Sound)
	{
		GEditor->PlayPreviewSound(Sound);
	}
	else
	{
		StopSound();
	}
}

void FAssetTypeActions_DialogueWave::StopSound()
{
	GEditor->ResetPreviewAudioComponent();
}

void FAssetTypeActions_DialogueWave::ExecuteCreateSoundCue(TArray<TWeakObjectPtr<UDialogueWave>> Objects, bool bCreateCueForEachDialogueWave)
{
	const FString DefaultSuffix = TEXT("_Cue");

	if (Objects.Num() == 1 || !bCreateCueForEachDialogueWave)
	{
		auto Object = Objects[0].Get();

		if (Object)
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
			Factory->InitialDialogueWaves = Objects;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USoundCue::StaticClass(), Factory);
		}
	}
	else
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if (Object)
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
				Factory->InitialDialogueWaves.Add(Object);

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USoundCue::StaticClass(), Factory);

				if (NewAsset)
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if (ObjectsToSync.Num() > 0)
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

const TArray<FText>& FAssetTypeActions_DialogueWave::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		FText(LOCTEXT("AssetDialogueSubMenu", "Dialogue"))
	};

	return SubMenus;
}
#undef LOCTEXT_NAMESPACE
