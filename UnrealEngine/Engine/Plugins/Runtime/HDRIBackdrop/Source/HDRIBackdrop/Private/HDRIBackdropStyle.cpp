// Copyright Epic Games, Inc. All Rights Reserved.

#include "HDRIBackdropStyle.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateTypes.h"
#include "Interfaces/IPluginManager.h"

TSharedPtr<FSlateStyleSet> FHDRIBackdropStyle::StyleSet;

void FHDRIBackdropStyle::Initialize()
{
	if (StyleSet.IsValid())
	{
		return;
	}

	StyleSet = MakeShared<FSlateStyleSet>(FName("HDRIBackdropStyle"));

	StyleSet->SetContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	StyleSet->SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

	FString IconPath = IPluginManager::Get().FindPlugin(TEXT("HDRIBackdrop"))->GetBaseDir() + TEXT("/Resources/HDRIBackdrop.png");
	StyleSet->Set("HDRIBackdrop.ModesThumbnail", new FSlateImageBrush(IconPath, FVector2D(40.0f, 40.0f)));
	StyleSet->Set("HDRIBackdrop.ModesIcon", new FSlateImageBrush(IconPath, FVector2D(16.0f, 16.0f)));

	FSlateStyleRegistry::RegisterSlateStyle(*StyleSet.Get());
}

void FHDRIBackdropStyle::Shutdown()
{
	if (StyleSet.IsValid())
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*StyleSet.Get());
		ensure(StyleSet.IsUnique());
		StyleSet.Reset();
	}
}