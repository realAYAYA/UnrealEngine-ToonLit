// Copyright Epic Games, Inc. All Rights Reserved.

#include "Modules/ModuleManager.h"

class FPixelStreamingPlayerEditorModule : public IModuleInterface
{
public:
private:
	/** IModuleInterface implementation */
	void StartupModule() override {}
	void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FPixelStreamingPlayerEditorModule, PixelStreamingPlayerEditor)
