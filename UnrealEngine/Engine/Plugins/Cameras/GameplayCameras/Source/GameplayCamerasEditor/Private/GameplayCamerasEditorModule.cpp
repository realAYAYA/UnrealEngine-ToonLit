// Copyright Epic Games, Inc. All Rights Reserved.

#include "GameplayCamerasEditorSettings.h"

#include "GameplayCamerasLiveEditManager.h"
#include "IGameplayCamerasModule.h"
#include "IGameplayCamerasEditorModule.h"
#include "ISettingsModule.h"
#include "Modules/ModuleManager.h"
#include "Styles/GameplayCamerasEditorStyle.h"
#include "Toolkits/CameraAssetEditorToolkit.h"
#include "Toolkits/CameraModeEditorToolkit.h"

#define LOCTEXT_NAMESPACE "GameplayCamerasEditor"

const FName IGameplayCamerasEditorModule::GameplayCamerasEditorAppIdentifier("GameplayCamerasEditorApp");

/**
 * Implements the FGameplayCamerasEditor module.
 */
class FGameplayCamerasEditorModule : public IGameplayCamerasEditorModule
{
public:
	FGameplayCamerasEditorModule()
	{
	}

	virtual void StartupModule() override
	{
		RegisterSettings();
		InitializeLiveEditManager();
	}

	virtual void ShutdownModule() override
	{
		UnregisterSettings();
		TeardownLiveEditManager();
	}

	virtual TSharedRef<ICameraAssetEditorToolkit> CreateCameraAssetEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCameraAsset* CameraAsset) override
	{
		TSharedRef<FCameraAssetEditorToolkit> Toolkit = MakeShareable(new FCameraAssetEditorToolkit(FGameplayCamerasEditorStyle::Get()));
		Toolkit->Initialize(Mode, InitToolkitHost, CameraAsset);
		return Toolkit;
	}

	virtual TSharedRef<ICameraModeEditorToolkit> CreateCameraModeEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UCameraMode* CameraMode) override
	{
		TSharedRef<FCameraModeEditorToolkit> Toolkit = MakeShareable(new FCameraModeEditorToolkit(FGameplayCamerasEditorStyle::Get()));
		Toolkit->Initialize(Mode, InitToolkitHost, CameraMode);
		return Toolkit;
	}

private:

	void RegisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "GameplayCamerasEditor",
				LOCTEXT("GameplayCamerasEditorProjectSettingsName", "Gameplay Cameras Editor"),
				LOCTEXT("GameplayCamerasEditorProjectSettingsDescription", "Configure the gameplay cameras editor."),
				GetMutableDefault<UGameplayCamerasEditorSettings>()
			);
		}
	}

	void UnregisterSettings()
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if (SettingsModule != nullptr)
		{
			SettingsModule->UnregisterSettings("Project", "Plugins", "GameplayCamerasEditor");
			SettingsModule->UnregisterSettings("Editor", "ContentEditors", "GameplayCamerasEditor");
		}
	}

	void InitializeLiveEditManager()
	{
		LiveEditManager = MakeShared<FGameplayCamerasLiveEditManager>();

		IGameplayCamerasModule& CamerasModule = FModuleManager::LoadModuleChecked<IGameplayCamerasModule>("GameplayCameras");
		CamerasModule.SetLiveEditManager(LiveEditManager);

		FCoreUObjectDelegates::GetPostGarbageCollect().AddRaw(this, &FGameplayCamerasEditorModule::OnPostGarbageCollection);
	}

	void TeardownLiveEditManager()
	{
		FCoreUObjectDelegates::GetPostGarbageCollect().RemoveAll(this);

		IGameplayCamerasModule& CamerasModule = FModuleManager::LoadModuleChecked<IGameplayCamerasModule>("GameplayCameras");
		CamerasModule.SetLiveEditManager(nullptr);

		LiveEditManager.Reset();
	}
	
	void OnPostGarbageCollection()
	{
		LiveEditManager->CleanUp();
	}

private:

	TSharedPtr<FGameplayCamerasLiveEditManager> LiveEditManager;
};

IMPLEMENT_MODULE(FGameplayCamerasEditorModule, GameplayCamerasEditor);

#undef LOCTEXT_NAMESPACE

