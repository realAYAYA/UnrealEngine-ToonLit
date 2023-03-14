// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Margin.h"
#include "Fonts/SlateFontInfo.h"
#include "Misc/Paths.h"
#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateBoxBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

/**
 * Implements the visual style of the template sequence editor.
 */
class FTemplateSequenceEditorStyle final : public FSlateStyleSet
{
public:
	
	FTemplateSequenceEditorStyle()
		: FSlateStyleSet("TemplateSequenceEditorStyle")
	{
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon24x24(24.0f, 24.0f);
		const FVector2D Icon48x48(48.0f, 48.0f);
		const FVector2D Icon64x64(64.0f, 64.0f);

		SetContentRoot(FPaths::EnginePluginsDir() / TEXT("MovieScene/TemplateSequence/Content"));

		// tab icons
		Set("TemplateSequenceEditor.Tabs.Sequencer", new IMAGE_BRUSH("icon_tab_sequencer_16x", Icon16x16));

		// asset thumbnail
		Set("ClassThumbnail.TemplateSequence", new IMAGE_BRUSH("TemplateSequence_16x", Icon16x16));
		Set("ClassThumbnail.TemplateSequence", new IMAGE_BRUSH("TemplateSequence_64x", Icon64x64));

		// toolbar icons
		Set("TemplateSequenceEditor.Chain", new IMAGE_BRUSH("Chain_16x", Icon16x16));
		Set("TemplateSequenceEditor.Chain.Small", new IMAGE_BRUSH("Chain_24x", Icon24x24));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	}

	virtual ~FTemplateSequenceEditorStyle()
	{
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	}

	static TSharedRef<FTemplateSequenceEditorStyle> Get()
	{
		if (!Singleton.IsValid())
		{
			Singleton = MakeShareable(new FTemplateSequenceEditorStyle);
		}
		return Singleton.ToSharedRef();
	}

private:

	static TSharedPtr<FTemplateSequenceEditorStyle> Singleton;
};

#undef IMAGE_BRUSH
