// Copyright Epic Games, Inc. All Rights Reserved.

#include "Text3DPrivate.h"
#include "Modules/ModuleManager.h"
#include "TextShaper.h"


DEFINE_LOG_CATEGORY(LogText3D)

#define LOCTEXT_NAMESPACE "FText3DModule"


FText3DModule::FText3DModule()
{
	FreeTypeLib = nullptr;
}

void FText3DModule::StartupModule()
{
	FT_Init_FreeType(&FreeTypeLib);
	FTextShaper::Initialize();
}

void FText3DModule::ShutdownModule()
{
	FT_Done_FreeType(FreeTypeLib);
	FreeTypeLib = nullptr;

	FTextShaper::Cleanup();
}

FT_Library FText3DModule::GetFreeTypeLibrary()
{
	const FText3DModule& Instance = FModuleManager::LoadModuleChecked<FText3DModule>("Text3D");
	return Instance.FreeTypeLib;
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FText3DModule, Text3D)
