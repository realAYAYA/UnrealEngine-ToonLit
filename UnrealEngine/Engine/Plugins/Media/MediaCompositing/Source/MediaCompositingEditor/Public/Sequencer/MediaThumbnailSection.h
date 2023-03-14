// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Math/RangeSet.h"
#include "Misc/Timespan.h"
#include "Sections/ThumbnailSection.h"
#include "Templates/SharedPointer.h"
#include "TrackEditorThumbnail/TrackEditorThumbnail.h"
#include "UObject/GCObject.h"

class FTrackEditorThumbnailPool;
class ISequencer;
class UMediaPlayer;
class UMediaTexture;
class UMovieSceneMediaSection;


/**
 * Implements a thumbnail section for media tracks.
 */
class MEDIACOMPOSITINGEDITOR_API FMediaThumbnailSection
	: public FGCObject
	, public FThumbnailSection
	, public ICustomThumbnailClient
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InSection The movie scene section associated with this thumbnail section.
	 * @param InThumbnailPool The thumbnail pool to use for drawing media frame thumbnails.
	 * @param InSequencer The Sequencer object that owns this section.
	 */
	FMediaThumbnailSection(UMovieSceneMediaSection& InSection, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<ISequencer> InSequencer);

	/** Virtual destructor. */
	virtual ~FMediaThumbnailSection();

public:

	//~ FGCObject interface

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FMediaThumbnailSection");
	}

public:

	//~ FThumbnailSection interface

	virtual FMargin GetContentPadding() const override;
	virtual float GetSectionHeight() const override;
	virtual FText GetSectionTitle() const override;
	virtual void SetSingleTime(double GlobalTime) override;
	virtual int32 OnPaintSection(FSequencerSectionPainter& InPainter) const override;
	virtual void Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	// ISequencerSection interface

	virtual void BeginResizeSection() override;
	virtual void ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime) override;
	virtual void BeginSlipSection() override;
	virtual void SlipSection(FFrameNumber SlipTime) override;

public:

	//~ ICustomThumbnailClient interface

	virtual void Draw(FTrackEditorThumbnail& TrackEditorThumbnail) override;
	virtual void Setup() override;

protected:

	/**
	 * Draw the section's film border decoration.
	 *
	 * @param InPainter The object that paints the geometry.
	 * @param SectionSize The size of the section (in local coordinates).
	 */
	void DrawFilmBorder(FSequencerSectionPainter& InPainter, FVector2D SectionSize) const;

	/**
	 * Draw indicators for where the media source is looping.
	 *
	 * @param InPainter The object that paints the geometry.
	 * @param SectionSize The size of the section (in local coordinates).
	 */
	void DrawLoopIndicators(FSequencerSectionPainter& InPainter, FTimespan MediaDuration, FVector2D SectionSize) const;

	/** Draw the caching state of the given media samples. */
	void DrawSampleStates(FSequencerSectionPainter& InPainter, FTimespan MediaDuration, FVector2D SectionSize, const TRangeSet<FTimespan>& RangeSet, const FLinearColor& Color) const;

	/**
	 * Draw the info about the current media.
	 *
	 * @param InPainter			The object that paints the geometry.
	 * @param MediaPlayer		The player to get media info from.
	 * @param SectionSize		The size of the section (in local coordinates).
	 */
	void DrawMediaInfo(FSequencerSectionPainter& InPainter, UMediaPlayer* MediaPlayer, FVector2D SectionSize) const;

	/**
	 * Get the media player that is used by the evaluation template.
	 *
	 * @return The media player, or nullptr if not found.
	 */
	UMediaPlayer* GetTemplateMediaPlayer() const;

private:

	/** The section object that owns this section. */
	TWeakObjectPtr<UMovieSceneMediaSection> SectionPtr;

	/** Cached start offset value valid only during resize */
	FFrameNumber InitialStartOffsetDuringResize;

	/** Cached start time valid only during resize */
	FFrameNumber InitialStartTimeDuringResize;
};
