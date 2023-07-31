// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"

#include "UVEditorAssetEditorImpl.h"

class FLayoutExtender;

/**
 * Besides the normal module things, the module class is also responsible for hooking the 
 * UV editor into existing menus.
 */
class UVEDITOR_API FUVEditorModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	DECLARE_EVENT_OneParam(FUVEditorModule, FOnRegisterLayoutExtensions, FLayoutExtender&);
	virtual FOnRegisterLayoutExtensions& OnRegisterLayoutExtensions() { return RegisterLayoutExtensions; }

protected:
	void RegisterMenus();

private:
	FOnRegisterLayoutExtensions	RegisterLayoutExtensions;

	/** StaticClass is not safe on shutdown, so we cache the name, and use this to unregister on shut down */
	TArray<FName> ClassesToUnregisterOnShutdown;

	TSharedPtr<UE::Geometry::FUVEditorAssetEditorImpl> UVEditorAssetEditor;
};
