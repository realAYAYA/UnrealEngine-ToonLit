//
// Copyright (C) Valve Corporation. All rights reserved.
//

#include "SteamAudioEditorModule.h"

#include "ISettingsModule.h"
#include "Engine/World.h"
#include "Styling/SlateStyleRegistry.h"
#include "Kismet/GameplayStatics.h"
#include "Async/Async.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "LevelEditorViewport.h"
#include "LevelEditor.h"
#include "PropertyEditorModule.h"
#include "Styling/SlateStyle.h"
#include "Interfaces/IPluginManager.h"
#include "ClassIconFinder.h"
#include "Editor.h"

#include "BakeIndirectWindow.h"
#include "TickableNotification.h"
#include "PhononScene.h"
#include "SteamAudioSettings.h"
#include "IAssetTools.h"
#include "PhononOcclusionSettingsFactory.h"
#include "PhononProbeVolume.h"
#include "PhononProbeVolumeDetails.h"
#include "PhononProbeComponent.h"
#include "PhononProbeComponentVisualizer.h"
#include "PhononReverbSettingsFactory.h"
#include "PhononScene.h"
#include "PhononSourceComponent.h"
#include "PhononSourceComponentDetails.h"
#include "PhononSourceComponentVisualizer.h"
#include "PhononSpatializationSettingsFactory.h"
#include "PhononCommon.h"
#include "SteamAudioEdMode.h"

#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorModeManager.h"
#include "HAL/PlatformFileManager.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogSteamAudioEditor);

IMPLEMENT_MODULE(SteamAudio::FSteamAudioEditorModule, SteamAudioEditor)


namespace
{
	static const FName AssetToolsName = TEXT("AssetTools");

	template <typename T>
	void AddAssetAction(IAssetTools& AssetTools, TArray<TSharedPtr<FAssetTypeActions_Base>>& AssetActions)
	{
		TSharedPtr<T> AssetAction = MakeShared<T>();
		TSharedPtr<FAssetTypeActions_Base> AssetActionBase = StaticCastSharedPtr<FAssetTypeActions_Base>(AssetAction);
		AssetTools.RegisterAssetTypeActions(AssetAction.ToSharedRef());
		AssetActions.Add(AssetActionBase);
	}
} // namespace <>

namespace SteamAudio
{
	//==============================================================================================================================================
	// FSteamAudioEditorModule
	//==============================================================================================================================================

	void FSteamAudioEditorModule::StartupModule()
	{
		// Ensure that Steam Audio has appropriate folders within which it can manage its data
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		
		if (!PlatformFile.DirectoryExists(*BasePath))
		{
			PlatformFile.CreateDirectory(*BasePath);
		}

		if (!PlatformFile.DirectoryExists(*RuntimePath))
		{
			PlatformFile.CreateDirectory(*RuntimePath);
		}

		if (!PlatformFile.DirectoryExists(*DynamicRuntimePath))
		{
			PlatformFile.CreateDirectory(*DynamicRuntimePath);
		}

		if (!PlatformFile.DirectoryExists(*EditorOnlyPath))
		{
			PlatformFile.CreateDirectory(*EditorOnlyPath);
		}

		if (!PlatformFile.DirectoryExists(*DynamicEditorOnlyPath))
		{
			PlatformFile.CreateDirectory(*DynamicEditorOnlyPath);
		}

		// Register detail customizations
		FPropertyEditorModule& PropertyModule = FModuleManager::LoadModuleChecked<FPropertyEditorModule>("PropertyEditor");

		PropertyModule.RegisterCustomClassLayout("PhononProbeVolume", FOnGetDetailCustomizationInstance::CreateStatic(&FPhononProbeVolumeDetails::MakeInstance));
		PropertyModule.RegisterCustomClassLayout("PhononSourceComponent",  FOnGetDetailCustomizationInstance::CreateStatic(&FPhononSourceComponentDetails::MakeInstance));

		// Extend the toolbar build menu with custom actions
		if (!IsRunningCommandlet())
		{
			FLevelEditorModule* LevelEditorModule = FModuleManager::LoadModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
			if (LevelEditorModule)
			{
				auto BuildMenuExtender = FLevelEditorModule::FLevelEditorMenuExtender::CreateRaw(this, &FSteamAudioEditorModule::OnExtendLevelEditorBuildMenu);

				LevelEditorModule->GetAllLevelEditorToolbarBuildMenuExtenders().Add(BuildMenuExtender);
			}
		}

		// Register plugin settings
		ISettingsModule* SettingsModule = FModuleManager::Get().GetModulePtr<ISettingsModule>("Settings");
		if (SettingsModule)
		{
			SettingsModule->RegisterSettings("Project", "Plugins", "Steam Audio", NSLOCTEXT("SteamAudio", "SteamAudio", "Steam Audio"),
				NSLOCTEXT("SteamAudio", "ConfigureSteamAudioSettings", "Configure Steam Audio settings"), GetMutableDefault<USteamAudioSettings>());
		}

		// Create bake indirect window
		BakeIndirectWindow = MakeShareable(new FBakeIndirectWindow());

		// Create and register custom slate style
		FString SteamAudioContent = IPluginManager::Get().FindPlugin(TEXT("SteamAudio"))->GetBaseDir() + "/Content";
		FVector2D Vec16 = FVector2D(16.0f, 16.0f);
		FVector2D Vec40 = FVector2D(40.0f, 40.0f);
		FVector2D Vec64 = FVector2D(64.0f, 64.0f);

		SteamAudioStyleSet = MakeShareable(new FSlateStyleSet("SteamAudio"));
		SteamAudioStyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
		SteamAudioStyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));
		SteamAudioStyleSet->Set("ClassIcon.PhononSourceComponent", new FSlateImageBrush(SteamAudioContent + "/S_PhononSource_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassIcon.PhononGeometryComponent", new FSlateImageBrush(SteamAudioContent + "/S_PhononGeometry_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassIcon.PhononMaterialComponent", new FSlateImageBrush(SteamAudioContent + "/S_PhononMaterial_16.png", Vec16));

		SteamAudioStyleSet->Set("ClassIcon.PhononSpatializationSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononSpatializationSourceSettings_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassThumbnail.PhononSpatializationSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononSpatializationSourceSettings_64.png", Vec64));
		
		SteamAudioStyleSet->Set("ClassIcon.PhononOcclusionSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononOcclusionSourceSettings_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassThumbnail.PhononOcclusionSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononOcclusionSourceSettings_64.png", Vec64));
		
		SteamAudioStyleSet->Set("ClassIcon.PhononReverbSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononReverbSourceSettings_16.png", Vec16));
		SteamAudioStyleSet->Set("ClassThumbnail.PhononReverbSourceSettings", new FSlateImageBrush(SteamAudioContent + "/S_PhononReverbSourceSettings_64.png", Vec64));

		SteamAudioStyleSet->Set("LevelEditor.SteamAudioMode", new FSlateImageBrush(SteamAudioContent + "/SteamAudio_EdMode_40.png", Vec40));
		SteamAudioStyleSet->Set("LevelEditor.SteamAudioMode.Small", new FSlateImageBrush(SteamAudioContent + "/SteamAudio_EdMode_16.png", Vec16));

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(AssetToolsName).Get();
		AddAssetAction<FAssetTypeActions_PhononReverbSettings>(AssetTools, AssetActions);
		AddAssetAction<FAssetTypeActions_PhononOcclusionSettings>(AssetTools, AssetActions);
		AddAssetAction<FAssetTypeActions_PhononSpatializationSettings>(AssetTools, AssetActions);

		FSlateStyleRegistry::RegisterSlateStyle(*SteamAudioStyleSet.Get());

		// Register the ed mode
		FEditorModeRegistry::Get().RegisterMode<FSteamAudioEdMode>(
			FSteamAudioEdMode::EM_SteamAudio,
			NSLOCTEXT("SteamAudio", "SteamAudioMode", "Steam Audio"),
			FSlateIcon(SteamAudioStyleSet->GetStyleSetName(), "LevelEditor.SteamAudioMode", "LevelEditor.SteamAudioMode.Small"),
			true);

		// Register component visualizers
		RegisterComponentVisualizer(UPhononSourceComponent::StaticClass()->GetFName(), MakeShareable(new FPhononSourceComponentVisualizer()));
		RegisterComponentVisualizer(UPhononProbeComponent::StaticClass()->GetFName(), MakeShareable(new FPhononProbeComponentVisualizer()));
	}

	void FSteamAudioEditorModule::ShutdownModule()
	{
		// Unregister component visualizers
		if (GUnrealEd)
		{
			for (FName& ClassName : RegisteredComponentClassNames)
			{
				GUnrealEd->UnregisterComponentVisualizer(ClassName);
			}
		}

		AssetActions.Reset();

		FSlateStyleRegistry::UnRegisterSlateStyle(*SteamAudioStyleSet.Get());
	}

	void FSteamAudioEditorModule::BakeIndirect()
	{
		BakeIndirectWindow->Invoke();
	}

	void FSteamAudioEditorModule::RegisterComponentVisualizer(const FName ComponentClassName, TSharedPtr<FComponentVisualizer> Visualizer)
	{
		if (GUnrealEd)
		{
			GUnrealEd->RegisterComponentVisualizer(ComponentClassName, Visualizer);
		}

		RegisteredComponentClassNames.Add(ComponentClassName);

		if (Visualizer.IsValid())
		{
			Visualizer->OnRegister();
		}
	}

	TSharedRef<FExtender> FSteamAudioEditorModule::OnExtendLevelEditorBuildMenu(const TSharedRef<FUICommandList> CommandList)
	{
		TSharedRef<FExtender> Extender(new FExtender());

		Extender->AddMenuExtension("LevelEditorNavigation", EExtensionHook::After, nullptr,
			FMenuExtensionDelegate::CreateRaw(this, &FSteamAudioEditorModule::CreateBuildMenu));

		return Extender;
	}

	void FSteamAudioEditorModule::CreateBuildMenu(FMenuBuilder& Builder)
	{
		FUIAction ActionBakeIndirect(FExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::BakeIndirect),
			FCanExecuteAction::CreateRaw(this, &FSteamAudioEditorModule::IsReadyToBakeIndirect));

		Builder.BeginSection("LevelEditorIR", NSLOCTEXT("SteamAudio", "SteamAudio", "Steam Audio"));

		Builder.AddMenuEntry(NSLOCTEXT("SteamAudio", "BakeIndirectSound", "Bake Indirect Sound..."),
			NSLOCTEXT("SteamAudio", "OpensIndirectBakingManager", "Opens indirect baking manager."), FSlateIcon(), ActionBakeIndirect,
			NAME_None, EUserInterfaceActionType::Button);

		Builder.EndSection();
	}

	bool FSteamAudioEditorModule::IsReadyToBakeIndirect() const
	{
		return true;
	}
}
