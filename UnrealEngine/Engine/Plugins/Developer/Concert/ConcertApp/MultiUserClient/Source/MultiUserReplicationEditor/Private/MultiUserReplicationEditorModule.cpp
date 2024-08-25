// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiUserReplicationEditorModule.h"

#include "MultiUserReplicationEditorStyle.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "FMultiUserReplicationEditorModule"

namespace UE::MultiUserReplicationEditor
{
	void FMultiUserReplicationEditorModule::StartupModule()
	{
		FMultiUserReplicationEditorStyle::Initialize();
	}

	void FMultiUserReplicationEditorModule::ShutdownModule()
	{
		FMultiUserReplicationEditorStyle::Shutdown();
	}
}

IMPLEMENT_MODULE(UE::MultiUserReplicationEditor::FMultiUserReplicationEditorModule, MultiUserReplicationEditor);
#undef LOCTEXT_NAMESPACE