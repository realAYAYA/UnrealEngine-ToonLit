// Copyright Epic Games, Inc. All Rights Reserved.

#include "CustomEditor.h"
#include "CustomEditorStyle.h"
#include "CustomEditorCommands.h"
#include "Toolbars/BlueprintToolbar.h"
#include "Toolbars/LevelToolbar.h"
#include "CustomFileWatcher.h"

#include "Misc/MessageDialog.h"
#include "ToolMenus.h"

DEFINE_LOG_CATEGORY(LogCustomEditor);


//static const FName CustomEditorTabName("CustomEditor");

#define LOCTEXT_NAMESPACE "FCustomEditorModule"

void FCustomEditorModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	
	FCustomEditorStyle::Initialize();
	FCustomEditorStyle::ReloadTextures();

	BlueprintToolbar = MakeShareable(new FBlueprintToolbar);
	LevelToolbar = MakeShareable(new FLevelToolbar);

	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCustomEditorModule::RegisterMenus));
	OnPostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FCustomEditorModule::OnPostEngineInit);

	//OnPostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FCustomEditorModule::OnPostEngineInit);

	// Todo 侦测文件变更导致的脚本编译
	ProtocolFileWatcher = MakeUnique<FCustomFileWatcher>();
	//ProtocolFileWatcher->WatchDir(FZProtocolModule::Get().GetJsDir());
	//ProtocolFileWatcher->GenProtocolTsDeclaration();
}

void FCustomEditorModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.

	FCoreDelegates::OnPostEngineInit.Remove(OnPostEngineInitHandle);

	UToolMenus::UnRegisterStartupCallback(this);

	UToolMenus::UnregisterOwner(this);

	FCustomEditorStyle::Shutdown();

	FCustomEditorCommands::Unregister();
}


void FCustomEditorModule::RegisterMenus()
{
	// Owner will be used for cleanup in call to UToolMenus::UnregisterOwner
	FToolMenuOwnerScoped OwnerScoped(this);

	// 添加到上方Window下拉菜单栏下
	{
		/*UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("LevelEditor.MainMenu.Window");
		{
			FToolMenuSection& Section = Menu->FindOrAddSection("WindowLayout");
			Section.AddMenuEntryWithCommandList(FCustomEditorCommands::Get().PluginAction, PluginCommands);
		}*/
	}
	
	LevelToolbar->Initialize();
}

void FCustomEditorModule::OnPostEngineInit()
{
	// LevelToolbar->Initialize();
	BlueprintToolbar->Initialize();
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FCustomEditorModule, CustomEditor)