// Copyright Epic Games, Inc. All Rights Reserved.

#include "AssetTypeActions_SoundCueTemplate.h"

#include "SoundCueTemplate.h"
#include "SoundCueTemplateFactory.h"

#include "AudioEditorSettings.h"
#include "Components/AudioComponent.h"
#include "ToolMenus.h"
#include "ToolMenuSection.h"
#include "ContentBrowserModule.h"
#include "ContentBrowserMenuContexts.h"
#include "IContentBrowserSingleton.h"
#include "ObjectEditorUtils.h"
#include "Sound/SoundWaveProcedural.h"

#define LOCTEXT_NAMESPACE "AssetTypeActions"

UClass* FAssetTypeActions_SoundCueTemplate::GetSupportedClass() const
{
	return USoundCueTemplate::StaticClass();
}

const TArray<FText>& FAssetTypeActions_SoundCueTemplate::GetSubMenus() const
{
	if (GetDefault<UAudioEditorSettings>()->bPinSoundCueTemplateInAssetMenu)
	{
		static const TArray<FText> AssetTypeActionSubMenu;
		return AssetTypeActionSubMenu;
	}

	static const TArray<FText> AssetTypeActionSubMenu
	{
		LOCTEXT("AssetSoundCueSubMenu", "Source")
	};
	return AssetTypeActionSubMenu;
}

void FAssetActionExtender_SoundCueTemplate::RegisterMenus()
{
	FToolMenuOwnerScoped MenuOwner("SoundCueTemplate");

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.SoundWave");
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

		Section.AddDynamicEntry("SoundCueAsset", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			const TAttribute<FText> Label = LOCTEXT("SoundWave_CreateSoundCueTemplate", "Create SoundCueTemplate");
			const TAttribute<FText> ToolTip = LOCTEXT("SoundWave_CreateSoundCueTemplateToolTip", "Creates a SoundCueTemplate from the selected sound waves.");
			const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SoundCue");
			const FToolMenuExecuteAction UIExecuteAction = FToolMenuExecuteAction::CreateStatic(&FAssetActionExtender_SoundCueTemplate::ExecuteCreateSoundCueTemplate);

			InSection.AddMenuEntry("SoundWave_CreateSoundCueTemplate", Label, ToolTip, Icon, UIExecuteAction);
		}));
	}

	{
		UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("ContentBrowser.AssetContextMenu.SoundCueTemplate");
		FToolMenuSection& Section = Menu->FindOrAddSection("GetAssetActions");

		Section.AddDynamicEntry("SoundCueTemplate", FNewToolMenuSectionDelegate::CreateLambda([](FToolMenuSection& InSection)
		{
			if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(InSection))
			{
				{
					const TAttribute<FText> Label = LOCTEXT("SoundCueTemplate_CopyToSoundCue", "Copy To Sound Cue");
					const TAttribute<FText> ToolTip = LOCTEXT("SoundCueTemplate_CopyToSoundCueTooltip", "Exports a Sound Cue Template to a Sound Cue.");
					const FSlateIcon Icon = FSlateIcon(FAppStyle::GetAppStyleSetName(), "ClassIcon.SoundCue");
					const FToolMenuExecuteAction UIExecuteAction = FToolMenuExecuteAction::CreateStatic(&FAssetActionExtender_SoundCueTemplate::ExecuteCopyToSoundCue);

					InSection.AddMenuEntry("SoundCueTemplate_CopyToSoundCue", Label, ToolTip, Icon, UIExecuteAction);
				}
			}
		}));
	}
}

void FAssetActionExtender_SoundCueTemplate::ExecuteCopyToSoundCue(const FToolMenuContext& MenuContext)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	
	if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
	{
		for (USoundCueTemplate* Object : Context->LoadSelectedObjects<USoundCueTemplate>())
		{
			FString Name;
			FString PackagePath;
			AssetToolsModule.Get().CreateUniqueAssetName(Object->GetOutermost()->GetName(), TEXT(""), PackagePath, Name);

			if (USoundCueTemplateCopyFactory* Factory = NewObject<USoundCueTemplateCopyFactory>())
			{
				Factory->SoundCueTemplate = TWeakObjectPtr<USoundCueTemplate>(Object);
				FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
				ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USoundCue::StaticClass(), Factory);
			}
		}
	}
}

void FAssetActionExtender_SoundCueTemplate::ExecuteCreateSoundCueTemplate(const struct FToolMenuContext& MenuContext)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::Get().LoadModuleChecked<FAssetToolsModule>("AssetTools");
	
	if (const UContentBrowserAssetContextMenuContext* Context = UContentBrowserAssetContextMenuContext::FindContextWithAssets(MenuContext))
	{
		TArray<USoundWave*> SoundWaves = Context->LoadSelectedObjectsIf<USoundWave>([](const FAssetData& AssetData) { return !AssetData.IsInstanceOf<USoundWaveProcedural>(); });
		if (!SoundWaves.IsEmpty())
		{
			FString PackagePath;
			FString Name;
			
			AssetToolsModule.Get().CreateUniqueAssetName(SoundWaves[0]->GetOutermost()->GetName(), TEXT(""), PackagePath, Name);

			USoundCueTemplateFactory* Factory = NewObject<USoundCueTemplateFactory>();
			Factory->SoundWaves = TArray<TWeakObjectPtr<USoundWave>>(SoundWaves);
			Factory->ConfigureProperties();
			Name = Factory->GetDefaultNewAssetName();

			FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
			ContentBrowserModule.Get().CreateNewAsset(Name, FPackageName::GetLongPackagePath(PackagePath), USoundCueTemplate::StaticClass(), Factory);
		}
	}
}

#undef LOCTEXT_NAMESPACE
