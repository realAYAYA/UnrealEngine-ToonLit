// Copyright Epic Games, Inc. All Rights Reserved.


#include "MediaFrameworkUtilitiesEditorModule.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

#include "Editor.h"
#include "Editor/UnrealEdEngine.h"
#include "LevelEditor.h"
#include "PropertyEditorDelegates.h"
#include "PropertyEditorModule.h"
#include "UObject/UObjectBase.h"

#include "AssetEditor/MediaProfileCommands.h"
#include "AssetTypeActions/AssetTypeActions_MediaBundle.h"
#include "AssetTypeActions/AssetTypeActions_MediaProfile.h"
#include "CaptureTab/SMediaFrameworkCapture.h"
#include "MediaBundleActorDetails.h"
#include "MediaBundleActorBase.h"
#include "MediaBundleFactoryNew.h"
#include "MediaFrameworkUtilitiesPlacement.h"
#include "Profile/MediaProfileCustomization.h"
#include "Profile/MediaProfile.h"
#include "Profile/MediaProfileBlueprintLibrary.h"
#include "Profile/MediaProfileSettings.h"
#include "Profile/MediaProfileSettingsCustomization.h"
#include "VideoInputTab/SMediaFrameworkVideoInput.h"
#include "UI/MediaFrameworkUtilitiesEditorStyle.h"
#include "UI/MediaProfileMenuEntry.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"



#define LOCTEXT_NAMESPACE "MediaFrameworkEditor"

DEFINE_LOG_CATEGORY(LogMediaFrameworkUtilitiesEditor);

/**
 * Implements the MediaPlayerEditor module.
 */
class FMediaFrameworkUtilitiesEditorModule : public IModuleInterface
{
public:
	FName NotificationBarIdentifier = TEXT("MediaProfile");

	//~ IModuleInterface interface

	virtual void StartupModule() override
	{
		if (GEditor)
		{
			FMediaProfileCommands::Register();
			FMediaFrameworkUtilitiesEditorStyle::Register();

			GEditor->ActorFactories.Add(NewObject<UActorFactoryMediaBundle>());

			FMediaFrameworkUtilitiesPlacement::RegisterPlacement();

			// Register AssetTypeActions
			auto RegisterAssetTypeAction = [this](const TSharedRef<IAssetTypeActions>& InAssetTypeAction)
			{
				IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
				RegisteredAssetTypeActions.Add(InAssetTypeAction);
				AssetTools.RegisterAssetTypeActions(InAssetTypeAction);
			};

			// register asset type actions
			RegisterAssetTypeAction(MakeShared<FAssetTypeActions_MediaBundle>());
			RegisterAssetTypeAction(MakeShared<FAssetTypeActions_MediaProfile>());

			// register detail panel customization
			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.RegisterCustomClassLayout(AMediaBundleActorBase::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMediaBundleActorDetails::MakeInstance));
			PropertyModule.RegisterCustomClassLayout(UMediaProfile::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMediaProfileCustomization::MakeInstance));
			PropertyModule.RegisterCustomClassLayout(UMediaProfileSettings::StaticClass()->GetFName(), FOnGetDetailCustomizationInstance::CreateStatic(&FMediaProfileSettingsCustomization::MakeInstance));

			{
				const IWorkspaceMenuStructure& MenuStructure = WorkspaceMenu::GetMenuStructure();
				TSharedRef<FWorkspaceItem> MediaBrowserGroup = MenuStructure.GetLevelEditorVirtualProductionCategory()->AddGroup(
					LOCTEXT("WorkspaceMenu_MediaCategory", "Media"),
					FSlateIcon(),
					true);

				SMediaFrameworkCapture::RegisterNomadTabSpawner(MediaBrowserGroup);
				SMediaFrameworkVideoInput::RegisterNomadTabSpawner(MediaBrowserGroup);
			}
			FMediaProfileMenuEntry::Register();

			{
				FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
				if (LevelEditorModule != nullptr)
				{
					FLevelEditorModule::FTitleBarItem Item;
					Item.Label = LOCTEXT("MediaProfileLabel", "MediaProfile: ");
					Item.Value = MakeAttributeLambda([]() { UObject* MediaProfile = UMediaProfileBlueprintLibrary::GetMediaProfile(); return MediaProfile ? FText::FromName(MediaProfile->GetFName()) : FText::GetEmpty(); });
					Item.Visibility = MakeAttributeLambda([]() { return GetDefault<UMediaProfileEditorSettings>()->bDisplayInMainEditor ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed; });
					LevelEditorModule->AddTitleBarItem(NotificationBarIdentifier, Item);
				}
			}
		}
	}

	virtual void ShutdownModule() override
	{
		if (!IsEngineExitRequested() && GEditor && UObjectInitialized())
		{
			FLevelEditorModule* LevelEditorModule = FModuleManager::GetModulePtr<FLevelEditorModule>("LevelEditor");
			if (LevelEditorModule != nullptr)
			{
				LevelEditorModule->RemoveTitleBarItem(NotificationBarIdentifier);
			}

			FMediaProfileMenuEntry::Unregister();
			SMediaFrameworkVideoInput::UnregisterNomadTabSpawner();
			SMediaFrameworkCapture::UnregisterNomadTabSpawner();

			FPropertyEditorModule& PropertyModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>("PropertyEditor");
			PropertyModule.UnregisterCustomClassLayout(UMediaProfileSettings::StaticClass()->GetFName());
			PropertyModule.UnregisterCustomClassLayout(UMediaProfile::StaticClass()->GetFName());
			PropertyModule.UnregisterCustomClassLayout(AMediaBundleActorBase::StaticClass()->GetFName());

			// Unregister AssetTypeActions
			FAssetToolsModule* AssetToolsModule = FModuleManager::GetModulePtr<FAssetToolsModule>("AssetTools");

			if (AssetToolsModule != nullptr)
			{
				IAssetTools& AssetTools = AssetToolsModule->Get();

				for (auto Action : RegisteredAssetTypeActions)
				{
					AssetTools.UnregisterAssetTypeActions(Action);
				}
			}

			FMediaFrameworkUtilitiesPlacement::UnregisterPlacement();

			GEditor->ActorFactories.RemoveAll([](const UActorFactory* ActorFactory) { return ActorFactory->IsA<UActorFactoryMediaBundle>(); });

			FMediaFrameworkUtilitiesEditorStyle::Unregister();
			FMediaProfileCommands::Unregister();
		}
	}

private:

	TArray<TSharedRef<IAssetTypeActions>> RegisteredAssetTypeActions;
};


IMPLEMENT_MODULE(FMediaFrameworkUtilitiesEditorModule, MediaFrameworkUtilitiesEditor);


#undef LOCTEXT_NAMESPACE
