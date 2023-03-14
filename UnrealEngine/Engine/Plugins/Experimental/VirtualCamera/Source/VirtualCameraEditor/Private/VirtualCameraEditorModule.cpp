// Copyright Epic Games, Inc. All Rights Reserved.

#include "VirtualCameraEditorModule.h"

#include "ActorFactories/ActorFactoryBlueprint.h"
#include "Filters/CustomClassFilterData.h"
#include "IPlacementModeModule.h"
#include "ISettingsModule.h"
#include "ISettingsSection.h"
#include "IVPUtilitiesEditorModule.h"
#include "LevelEditor.h"
#include "LevelEditorOutlinerSettings.h"
#include "Modules/ModuleManager.h"
#include "SimpleVirtualCamera.h"
#include "VirtualCameraActor.h"
#include "VirtualCameraUserSettings.h"

#define LOCTEXT_NAMESPACE "FVirtualCameraEditorModule"

DEFINE_LOG_CATEGORY_STATIC(LogVirtualCameraEditor, Log, All);

class FVirtualCameraEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		RegisterSettings();
		RegisterPlacementModeItems();
		RegisterOutlinerFilters();
	}

	virtual void ShutdownModule() override
	{
		UnregisterSettings();
	}

	void RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			ISettingsSectionPtr SettingsSection = SettingsModule->RegisterSettings("Project", "Plugins", "VirtualCamera",
				LOCTEXT("VirtualCameraUserSettingsName", "Virtual Camera"),
				LOCTEXT("VirtualCameraUserSettingsDescription", "Configure the Virtual Camera settings."),
				GetMutableDefault<UVirtualCameraUserSettings>());
		}
	}

	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "VirtualCamera");
		}
	}

	void RegisterPlacementModeItems()
	{
		if (const FPlacementCategoryInfo* Info = IVPUtilitiesEditorModule::Get().GetVirtualProductionPlacementCategoryInfo()
			; Info && GEditor)
		{
			FAssetData VirtualCamera2ActorAssetData(
				TEXT("/VirtualCamera/VCamActor"),
				TEXT("/VirtualCamera"),
				TEXT("VCamActor"),
				FTopLevelAssetPath(TEXT("/Script/Engine"), TEXT("Blueprint"))
			);

			// Makes it appear in the VP category ...
			IPlacementModeModule::Get().RegisterPlaceableItem(Info->UniqueHandle, MakeShared<FPlaceableItem>(
				*UActorFactoryBlueprint::StaticClass(),
				VirtualCamera2ActorAssetData,
				FName("ClassThumbnail.CameraActor"),
				FName("ClassIcon.CameraActor"),
				TOptional<FLinearColor>(),
				TOptional<int32>(),
				NSLOCTEXT("PlacementMode", "VCam Actor", "VCam Actor")
				)
			);

			// ... but if you search for it by text this is needed to make it show up (without having the user load it manually).
			// The search filters everything in the FBuiltInPlacementCategories::AllClasses category only;
			// it contains 1. loaded BP classes and 2. specialized actor factories. This manual load adds it to case 1. 
			const UClass* VCamActorBlueprintClass = LoadClass<UObject>(nullptr, TEXT("/VirtualCamera/VCamActor.VCamActor_C"));
			UE_CLOG(VCamActorBlueprintClass == nullptr, LogVirtualCameraEditor, Warning, TEXT("Failed to load '/VirtualCamera/VCamActor.VCamActor_C'. Has the Blueprint been moved?"));
		}
	}

	void RegisterOutlinerFilters()
	{
		FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>(TEXT("LevelEditor"));
		
		if(TSharedPtr<FFilterCategory> VPFilterCategory = LevelEditorModule.GetOutlinerFilterCategory(FLevelEditorOutlinerBuiltInCategories::VirtualProduction()))
		{
			TSharedRef<FCustomClassFilterData> CineCameraActorClassData = MakeShared<FCustomClassFilterData>(ACineCameraActor::StaticClass(), VPFilterCategory, FLinearColor::White);
			LevelEditorModule.AddCustomClassFilterToOutliner(CineCameraActorClassData);
		}
	}
};
#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FVirtualCameraEditorModule, VirtualCameraEditor)


