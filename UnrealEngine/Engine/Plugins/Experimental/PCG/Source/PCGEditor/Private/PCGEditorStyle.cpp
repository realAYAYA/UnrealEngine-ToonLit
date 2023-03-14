// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGEditorStyle.h"

#include "Misc/Paths.h"
#include "Styling/SlateStyleMacros.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/StyleColors.h"

void FPCGEditorStyle::Register()
{
	FSlateStyleRegistry::RegisterSlateStyle(Get());
}

void FPCGEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(Get());
}

FPCGEditorStyle::FPCGEditorStyle() : FSlateStyleSet("PCGEditorStyle")
{
    static const FVector2D Icon20x20(20.0f, 20.0f);
	
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));

	Set("PCG.NodeOverlay.Debug", new CORE_IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", Icon20x20, FSlateColor(FColor::Cyan)));
	Set("PCG.NodeOverlay.Inspect", new CORE_IMAGE_BRUSH_SVG("Starship/Blueprints/Breakpoint_Valid", Icon20x20, FSlateColor(FColor::Orange)));
}

const FPCGEditorStyle& FPCGEditorStyle::Get()
{
	static FPCGEditorStyle Instance;
	return Instance;
}
