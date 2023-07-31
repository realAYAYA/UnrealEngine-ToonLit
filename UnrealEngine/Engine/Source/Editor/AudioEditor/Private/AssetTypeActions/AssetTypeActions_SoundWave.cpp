// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions/AssetTypeActions_SoundWave.h"

#include "AssetRegistry/AssetData.h"
#include "AssetToolsModule.h"
#include "Containers/UnrealString.h"
#include "ContentBrowserModule.h"
#include "Delegates/Delegate.h"
#include "EditorFramework/AssetImportData.h"
#include "Factories/DialogueWaveFactory.h"
#include "Factories/SoundCueFactoryNew.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GenericPlatform/ICursor.h"
#include "HAL/Platform.h"
#include "HAL/PlatformCrt.h"
#include "IAssetTools.h"
#include "IContentBrowserSingleton.h"
#include "Input/Reply.h"
#include "Layout/Margin.h"
#include "Layout/Visibility.h"
#include "Misc/Attribute.h"
#include "Misc/Optional.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PropertyCustomizationHelpers.h"
#include "Sound/DialogueVoice.h"
#include "Sound/DialogueWave.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateColor.h"
#include "Templates/Casts.h"
#include "Textures/SlateIcon.h"
#include "ToolMenuSection.h"
#include "Toolkits/IToolkit.h"
#include "Toolkits/SimpleAssetEditor.h"
#include "Types/SlateEnums.h"
#include "UObject/Object.h"
#include "UObject/ObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"

#include <utility>

class IToolkitHost;
class SWidget;
class UClass;
struct FSlateBrush;

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundWave::GetSupportedClass() const
{
	return USoundWave::StaticClass();
}

void FAssetTypeActions_SoundWave::GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section)
{
	FAssetTypeActions_SoundBase::GetActions(InObjects, Section);

	TArray<TWeakObjectPtr<USoundWave>> SoundNodes = GetTypedWeakObjectPtrs<USoundWave>(InObjects);
	bool bCreateCueForEachSoundWave = true;

	if (SoundNodes.Num() == 1)
	{
		Section.AddMenuEntry(
			"SoundWave_CreateCue",
			LOCTEXT("SoundWave_CreateCue", "Create Cue"),
			LOCTEXT("SoundWave_CreateCueTooltip", "Creates a sound cue using this sound wave."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SoundCue"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_SoundWave::ExecuteCreateSoundCue, SoundNodes, bCreateCueForEachSoundWave),
				FCanExecuteAction()
			)
		);
	}
	else
	{
		bCreateCueForEachSoundWave = false;
		Section.AddMenuEntry(
			"SoundWave_CreateSingleCue",
			LOCTEXT("SoundWave_CreateSingleCue", "Create Single Cue"),
			LOCTEXT("SoundWave_CreateSingleCueTooltip", "Creates a single sound cue using these sound waves."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SoundCue"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_SoundWave::ExecuteCreateSoundCue, SoundNodes, bCreateCueForEachSoundWave),
				FCanExecuteAction()
			)
		);

		bCreateCueForEachSoundWave = true;
		Section.AddMenuEntry(
			"SoundWave_CreateMultiCue",
			LOCTEXT("SoundWave_CreateMultiCue", "Create Multiple Cues"),
			LOCTEXT("SoundWave_CreateMultiCueTooltip", "Creates multiple sound cues, one from each selected sound wave."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SoundCue"),
			FUIAction(
				FExecuteAction::CreateSP(this, &FAssetTypeActions_SoundWave::ExecuteCreateSoundCue, SoundNodes, bCreateCueForEachSoundWave),
				FCanExecuteAction()
			)
		);
	}

	Section.AddSubMenu(
		"SoundWave_CreateDialogue",
		LOCTEXT("SoundWave_CreateDialogue", "Create Dialogue"),
		LOCTEXT("SoundWave_CreateDialogueTooltip", "Creates a dialogue wave using this sound wave."),
		FNewMenuDelegate::CreateSP(this, &FAssetTypeActions_SoundWave::FillVoiceMenu, SoundNodes));
}

void FAssetTypeActions_SoundWave::GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const
{
	for (auto& Asset : TypeAssets)
	{
		const auto SoundWave = CastChecked<USoundWave>(Asset);
		SoundWave->AssetImportData->ExtractFilenames(OutSourceFilePaths);
	}
}

void FAssetTypeActions_SoundWave::OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<IToolkitHost> EditWithinLevelEditor )
{
	FSimpleAssetEditor::CreateEditor(EToolkitMode::Standalone, EditWithinLevelEditor, InObjects);
}

void FAssetTypeActions_SoundWave::ExecuteCreateSoundCue(TArray<TWeakObjectPtr<USoundWave>> Objects, bool bCreateCueForEachSoundWave)
{
	const FString DefaultSuffix = TEXT("_Cue");

	if ( Objects.Num() == 1 || !bCreateCueForEachSoundWave)
	{
		auto Object = Objects[0].Get();

		if ( Object )
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
			Factory->InitialSoundWaves = Objects;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USoundCue::StaticClass(), Factory);
		}
	}
	else if ( bCreateCueForEachSoundWave )
	{
		TArray<UObject*> ObjectsToSync;

		for (auto ObjIt = Objects.CreateConstIterator(); ObjIt; ++ObjIt)
		{
			auto Object = (*ObjIt).Get();
			if ( Object )
			{
				FString Name;
				FString PackageName;
				CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackageName, Name);

				// Create the factory used to generate the asset
				USoundCueFactoryNew* Factory = NewObject<USoundCueFactoryNew>();
				Factory->InitialSoundWaves.Add(Object);

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), USoundCue::StaticClass(), Factory);

				if ( NewAsset )
				{
					ObjectsToSync.Add(NewAsset);
				}
			}
		}

		if ( ObjectsToSync.Num() > 0 )
		{
			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().SyncBrowserToAssets(ObjectsToSync);
		}
	}
}

void FAssetTypeActions_SoundWave::ExecuteCreateDialogueWave(const struct FAssetData& AssetData, TArray<TWeakObjectPtr<USoundWave>> Objects)
{
	const FString DefaultSuffix = TEXT("_Dialogue");

	UDialogueVoice* DialogueVoice = Cast<UDialogueVoice>(AssetData.GetAsset());

	if (Objects.Num() == 1)
	{
		auto Object = Objects[0].Get();

		if (Object)
		{
			// Determine an appropriate name
			FString Name;
			FString PackagePath;
			CreateUniqueAssetName(Object->GetOutermost()->GetName(), DefaultSuffix, PackagePath, Name);

			// Create the factory used to generate the asset
			UDialogueWaveFactory* Factory = NewObject<UDialogueWaveFactory>();
			Factory->InitialSoundWave = Object;
			Factory->InitialSpeakerVoice = DialogueVoice;
			Factory->HasSetInitialTargetVoice = true;

			FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), UDialogueWave::StaticClass(), Factory);
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
				UDialogueWaveFactory* Factory = NewObject<UDialogueWaveFactory>();
				Factory->InitialSoundWave = Object;
				Factory->InitialSpeakerVoice = DialogueVoice;
				Factory->HasSetInitialTargetVoice = true;

				FAssetToolsModule& AssetToolsModule = FModuleManager::GetModuleChecked<FAssetToolsModule>("AssetTools");
				UObject* NewAsset = AssetToolsModule.Get().CreateAsset(Name, FPackageName::GetLongPackagePath(PackageName), UDialogueWave::StaticClass(), Factory);

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

void FAssetTypeActions_SoundWave::FillVoiceMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USoundWave>> Objects)
{
	TArray<const UClass*> AllowedClasses;
	AllowedClasses.Add(UDialogueVoice::StaticClass());

	TSharedRef<SWidget> VoicePicker = PropertyCustomizationHelpers::MakeAssetPickerWithMenu(
		FAssetData(),
		false, 
		AllowedClasses,
		PropertyCustomizationHelpers::GetNewAssetFactoriesForClasses(AllowedClasses),
		FOnShouldFilterAsset(),
		FOnAssetSelected::CreateSP(this, &FAssetTypeActions_SoundWave::ExecuteCreateDialogueWave, Objects),
		FSimpleDelegate());

	MenuBuilder.AddWidget(VoicePicker, FText::GetEmpty(), false);
}

TSharedPtr<SWidget> FAssetTypeActions_SoundWave::GetThumbnailOverlay(const FAssetData& AssetData) const
{
	auto OnGetDisplayBrushLambda = [this, AssetData]() -> const FSlateBrush*
	{
		if (IsSoundPlaying(AssetData))
		{
			return FAppStyle::GetBrush("MediaAsset.AssetActions.Stop.Large");
		}

		return FAppStyle::GetBrush("MediaAsset.AssetActions.Play.Large");
	};

	auto OnClickedLambda = [this, AssetData]() -> FReply
	{
		if (IsSoundPlaying(AssetData))
		{
			StopSound();
		}
		else
		{
			// Load and play sound
			PlaySound(Cast<USoundBase>(AssetData.GetAsset()));
		}
		return FReply::Handled();
	};

	auto OnToolTipTextLambda = [this, AssetData]() -> FText
	{
		if (IsSoundPlaying(AssetData))
		{
			return LOCTEXT("Blueprint_StopSoundToolTip", "Stop selected Sound Wave");
		}

		return LOCTEXT("Blueprint_PlaySoundToolTip", "Play selected Sound Wave");
	};

	TSharedPtr<SBox> Box;
	SAssignNew(Box, SBox)
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(FMargin(2));

	auto OnGetVisibilityLambda = [this, Box, AssetData]() -> EVisibility
	{
		if (Box.IsValid() && (Box->IsHovered() || IsSoundPlaying(AssetData)))
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

const TArray<FText>& FAssetTypeActions_SoundWave::GetSubMenus() const
{
	static const TArray<FText> SubMenus
	{
		FText(LOCTEXT("AssetSoundSourceSubMenu", "Source"))
	};

	return SubMenus;
}
#undef LOCTEXT_NAMESPACE
