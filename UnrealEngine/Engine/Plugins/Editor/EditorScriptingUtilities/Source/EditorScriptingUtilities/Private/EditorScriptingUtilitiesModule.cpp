// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"

#include "EditorPythonExecuter.h"
#include "Modules/ModuleManager.h"

/**
* The public interface of the DatasmithImporter module
*/
class FEditorScriptingUtilitiesModule : public IModuleInterface
{
	virtual void StartupModule() override
	{
		FEditorPythonExecuter::OnStartupModule();
	}

	virtual void ShutdownModule() override
	{
		FEditorPythonExecuter::OnShutdownModule();
	}
};


IMPLEMENT_MODULE(FEditorScriptingUtilitiesModule, EditorScriptingUtilities)
