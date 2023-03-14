// Copyright Epic Games, Inc. All Rights Reserved.

#include "SoundCueTemplatesEditorModule.h"

#include "Modules/ModuleManager.h"
#include "AssetToolsModule.h"
#include "ToolMenus.h"
#include "HAL/LowLevelMemTracker.h"

#include "AssetTypeActions_SoundCueTemplate.h"

void FSoundCueTemplatesEditorModule::StartupModule()
{
	LLM_SCOPE(ELLMTag::AudioMisc);
	RegisterAssetActions();
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FSoundCueTemplatesEditorModule::RegisterMenus));
}

void FSoundCueTemplatesEditorModule::ShutdownModule()
{
	LLM_SCOPE(ELLMTag::AudioMisc);
	UToolMenus::UnRegisterStartupCallback(this);
	UToolMenus::UnregisterOwner(UE_MODULE_NAME);
}

void FSoundCueTemplatesEditorModule::RegisterAssetActions()
{
	LLM_SCOPE(ELLMTag::AudioMisc);
	// Register the audio editor asset type actions
	IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

	AssetTools.RegisterAssetTypeActions(MakeShared<FAssetTypeActions_SoundCueTemplate>());
}

void FSoundCueTemplatesEditorModule::RegisterMenus()
{
	FAssetActionExtender_SoundCueTemplate::RegisterMenus();
}

IMPLEMENT_MODULE(FSoundCueTemplatesEditorModule, SoundCueTemplatesEditor);
