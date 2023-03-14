// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosCachingEditorStyle.h"

#include "Styling/SlateTypes.h"
#include "Styling/CoreStyle.h"
#include "Styling/AppStyle.h"
#include "Styling/SlateStyleMacros.h"

FName FChaosCachingEditorStyle::StyleName("ChaosCachingEditorStyle");

FChaosCachingEditorStyle::FChaosCachingEditorStyle()
	: FSlateStyleSet(StyleName)
{
	const FVector2D IconSize(20.0f, 20.0f);
	const FVector2D LabelIconSize(16.0f, 16.0f);

	SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Experimental/ChaosCaching/Content"));
	SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Editor/Slate"));
	Set("ChaosCachingEditor.Fracture", new IMAGE_BRUSH_SVG("fracture", LabelIconSize));

	FSlateStyleRegistry::RegisterSlateStyle(*this);
}

FChaosCachingEditorStyle::~FChaosCachingEditorStyle()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*this);
}

FChaosCachingEditorStyle& FChaosCachingEditorStyle::Get()
{
	static FChaosCachingEditorStyle Inst;
	return Inst;
}


