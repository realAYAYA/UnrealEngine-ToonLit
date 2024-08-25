// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Misc/Paths.h"
#include "Brushes/SlateBorderBrush.h"
#include "Brushes/SlateImageBrush.h"
#include "Interfaces/IPluginManager.h"
#include "Styling/SlateStyleRegistry.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleMacros.h"

/**
 * Implements the visual style of the messaging debugger UI.
 */
class FLevelSequenceEditorStyle final
	: public FSlateStyleSet
{
public:

	/** Default constructor. */
	 FLevelSequenceEditorStyle()
		 : FSlateStyleSet("LevelSequenceEditorStyle")
	 {
		const FVector2D Icon16x16(16.0f, 16.0f);
		const FVector2D Icon20x20(20.0f, 20.0f);
		const FVector2D Icon40x40(40.0f, 40.0f);
		const FVector2D Icon48x48(48.0f, 48.0f);

		const FString ContentDir = IPluginManager::Get().FindPlugin(TEXT("LevelSequenceEditor"))->GetContentDir();

		SetContentRoot(ContentDir);
		SetCoreContentRoot(FPaths::EngineContentDir() / TEXT("Slate"));

		// tab icons
		Set("LevelSequenceEditor.Tabs.Sequencer", new IMAGE_BRUSH("icon_tab_sequencer_16x", Icon16x16));

		Set("LevelSequenceEditor.PossessNewActor", new IMAGE_BRUSH_SVG("ActorToSequencer", Icon16x16));
		Set("LevelSequenceEditor.PossessNewActor.Small", new IMAGE_BRUSH_SVG("ActorToSequencer", Icon16x16));

		Set("LevelSequenceEditor.CreateNewLevelSequenceInLevel", new IMAGE_BRUSH_SVG("LevelSequence", Icon16x16));
		Set("LevelSequenceEditor.CreateNewLevelSequenceInLevel.Small", new IMAGE_BRUSH_SVG("LevelSequence", Icon16x16));

		Set("LevelSequenceEditor.CreateNewLevelSequenceWithShotsInLevel", new IMAGE_BRUSH_SVG("LevelSequenceWithShots", Icon16x16));
		Set("LevelSequenceEditor.CreateNewLevelSequenceWithShotsInLevel.Small", new IMAGE_BRUSH_SVG("LevelSequenceWithShots", Icon16x16));
		
		Set("LevelSequenceEditor.CinematicViewportPlayMarker", new IMAGE_BRUSH("CinematicViewportPlayMarker", FVector2D(11, 6)));
		Set("LevelSequenceEditor.CinematicViewportRangeStart", new BORDER_BRUSH("CinematicViewportRangeStart", FMargin(1.f,.3f,0.f,.6f)));
		Set("LevelSequenceEditor.CinematicViewportRangeEnd", new BORDER_BRUSH("CinematicViewportRangeEnd", FMargin(0.f,.3f,1.f,.6f)));

		Set("LevelSequenceEditor.CinematicViewportTransportRangeKey", new IMAGE_BRUSH("CinematicViewportTransportRangeKey", FVector2D(7.f, 7.f)));

		Set("LevelSequenceEditor.SaveAs", new IMAGE_BRUSH("Icon_Sequencer_SaveAs_48x", Icon48x48));
		Set("LevelSequenceEditor.ImportFBX", new IMAGE_BRUSH("Icon_Sequencer_ImportFBX_48x", Icon48x48));
		Set("LevelSequenceEditor.ExportFBX", new IMAGE_BRUSH("Icon_Sequencer_ExportFBX_48x", Icon48x48));

		Set("FilmOverlay.DefaultThumbnail", new IMAGE_BRUSH("DefaultFilmOverlayThumbnail", FVector2D(36, 24)));

		Set("FilmOverlay.Disabled", new IMAGE_BRUSH("FilmOverlay.Disabled", FVector2D(36, 24)));
		Set("FilmOverlay.2x2Grid", new IMAGE_BRUSH("FilmOverlay.2x2Grid", FVector2D(36, 24)));
		Set("FilmOverlay.3x3Grid", new IMAGE_BRUSH("FilmOverlay.3x3Grid", FVector2D(36, 24)));
		Set("FilmOverlay.Crosshair", new IMAGE_BRUSH("FilmOverlay.Crosshair", FVector2D(36, 24)));
		Set("FilmOverlay.Rabatment", new IMAGE_BRUSH("FilmOverlay.Rabatment", FVector2D(36, 24)));

		FSlateStyleRegistry::RegisterSlateStyle(*this);
	 }

	 /** Virtual destructor. */
	 virtual ~FLevelSequenceEditorStyle()
	 {
		FSlateStyleRegistry::UnRegisterSlateStyle(*this);
	 }

	static TSharedRef<FLevelSequenceEditorStyle> Get()
	{
		if (!Singleton.IsValid())
		{
			Singleton = MakeShareable(new FLevelSequenceEditorStyle);
		}
		return Singleton.ToSharedRef();
	}

private:
	static TSharedPtr<FLevelSequenceEditorStyle> Singleton;
};


#undef IMAGE_BRUSH
#undef BOX_BRUSH
#undef BORDER_BRUSH
