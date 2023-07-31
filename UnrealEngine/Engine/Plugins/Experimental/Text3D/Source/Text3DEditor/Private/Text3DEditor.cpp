// Copyright Epic Games, Inc. All Rights Reserved.

#include "Text3DEditor.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"


IMPLEMENT_MODULE(FText3DEditorModule, Text3DEditor)

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(StyleSet->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

void FText3DEditorModule::StartupModule()
{
	FString RootDir = IPluginManager::Get().FindPlugin(TEXT("Text3D"))->GetBaseDir() / TEXT("Resources");

	StyleSet = MakeUnique<FSlateStyleSet>(FName(TEXT("Text3DStyle")));
	StyleSet->SetContentRoot(RootDir);

	StyleSet->Set("ClassIcon.Text3DActor", new IMAGE_BRUSH("Text3DActor_16x", FVector2D (16.0f, 16.0f)));
	StyleSet->Set("ClassThumbnail.Text3DActor", new IMAGE_BRUSH("Text3DActor_64x", FVector2D (64.0f, 64.0f)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void FText3DEditorModule::ShutdownModule()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
	StyleSet.Reset();
}

