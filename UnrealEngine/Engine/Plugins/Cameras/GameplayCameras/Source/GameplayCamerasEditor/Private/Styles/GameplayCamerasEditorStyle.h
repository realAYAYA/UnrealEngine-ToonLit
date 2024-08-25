// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Paths.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

/**
 * Implements the visual style of the gameplay cameras editors.
 */
class FGameplayCamerasEditorStyle final : public FSlateStyleSet
{
public:
	
	FGameplayCamerasEditorStyle()
		: FSlateStyleSet("GameplayCamerasEditorStyle")
	{
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon24x24(24.0f, 24.0f);
		const FVector2D Icon48x48(48.0f, 48.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);

		//SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Cameras/GameplayCameras/Content"));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	virtual ~FGameplayCamerasEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

	static TSharedRef<FGameplayCamerasEditorStyle> Get()
	{
		if (!Singleton.IsValid())
		{
			Singleton = MakeShareable(new FGameplayCamerasEditorStyle);
		}
		return Singleton.ToSharedRef();
	}

private:

	static TSharedPtr<FGameplayCamerasEditorStyle> Singleton;
};

#undef IMAGE_BRUSH

