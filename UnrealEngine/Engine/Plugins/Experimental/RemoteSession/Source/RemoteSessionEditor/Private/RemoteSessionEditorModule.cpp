// Copyright Epic Games, Inc. All Rights Reserved.

#include "RemoteSessionEditorModule.h"
#include "RemoteSessionEditorStyle.h"

#include "Modules/ModuleManager.h"
#include "Widgets/SRemoteSessionStream.h"

LLM_DEFINE_TAG(RemoteSession_RemoteSessionEditor);
#define LOCTEXT_NAMESPACE "FRemoteSessionEditorModule"

class FRemoteSessionEditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		LLM_SCOPE_BYTAG(RemoteSession_RemoteSessionEditor);

		FRemoteSessionEditorStyle::Register();
		SRemoteSessionStream::RegisterNomadTabSpawner();
	}

	virtual void ShutdownModule() override
	{
		LLM_SCOPE_BYTAG(RemoteSession_RemoteSessionEditor);

		if (!IsEngineExitRequested() && UObjectInitialized())
		{
			FRemoteSessionEditorStyle::Unregister();
			SRemoteSessionStream::UnregisterNomadTabSpawner();
		}
	}
};

IMPLEMENT_MODULE(FRemoteSessionEditorModule, RemoteSessionEditor)

#undef LOCTEXT_NAMESPACE
