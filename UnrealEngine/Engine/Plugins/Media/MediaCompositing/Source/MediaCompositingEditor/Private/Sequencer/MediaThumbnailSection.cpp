// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MediaThumbnailSection.h"

#include "Fonts/FontMeasure.h"
#include "Styling/AppStyle.h"
#include "IMediaCache.h"
#include "IMediaTracks.h"
#include "ISequencer.h"
#include "MediaPlayer.h"
#include "MediaPlayerFacade.h"
#include "MediaSource.h"
#include "MediaTexture.h"
#include "MovieScene.h"
#include "MovieSceneMediaSection.h"
#include "MovieSceneTimeHelpers.h"
#include "Rendering/DrawElements.h"
#include "SequencerSectionPainter.h"
#include "TrackEditorThumbnail/TrackEditorThumbnailPool.h"
#include "Compilation/MovieSceneCompiledDataManager.h"
#include "CommonMovieSceneTools.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "TimeToPixel.h"

#include "MovieSceneMediaData.h"


#define LOCTEXT_NAMESPACE "FMediaThumbnailSection"


/* FMediaThumbnailSection structors
 *****************************************************************************/

FMediaThumbnailSection::FMediaThumbnailSection(UMovieSceneMediaSection& InSection, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, TSharedPtr<ISequencer> InSequencer)
	: FThumbnailSection(InSequencer, InThumbnailPool, this, InSection)
	, SectionPtr(&InSection)
{
	TimeSpace = ETimeSpace::Local;
}


FMediaThumbnailSection::~FMediaThumbnailSection()
{
}


/* FGCObject interface
 *****************************************************************************/

void FMediaThumbnailSection::AddReferencedObjects(FReferenceCollector& Collector)
{
}


/* FThumbnailSection interface
 *****************************************************************************/

FMargin FMediaThumbnailSection::GetContentPadding() const
{
	return FMargin(8.0f, 15.0f);
}


float FMediaThumbnailSection::GetSectionHeight() const
{
	return FThumbnailSection::GetSectionHeight() + 2 * 9.0f; // make space for the film border
}


FText FMediaThumbnailSection::GetSectionTitle() const
{
	UMovieSceneMediaSection* MediaSection = CastChecked<UMovieSceneMediaSection>(Section);
	UMediaSource* MediaSource = MediaSection->GetMediaSource();

	if (MediaSource == nullptr)
	{
		return LOCTEXT("NoSequence", "Empty");
	}

	return FText::FromString(MediaSource->GetFName().ToString());
}


int32 FMediaThumbnailSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	// draw background
	InPainter.LayerId = InPainter.PaintSectionBackground();

	FVector2D SectionSize = InPainter.SectionGeometry.GetLocalSize();
	FSlateClippingZone ClippingZone(InPainter.SectionClippingRect.InsetBy(FMargin(1.0f)));
	
	InPainter.DrawElements.PushClip(ClippingZone);
	{
		DrawFilmBorder(InPainter, SectionSize);
	}
	InPainter.DrawElements.PopClip();

	// draw thumbnails
	int32 LayerId = FThumbnailSection::OnPaintSection(InPainter) + 1;

	UMediaPlayer* MediaPlayer = GetTemplateMediaPlayer();

	if (MediaPlayer == nullptr)
	{
		return LayerId;
	}

	// draw overlays
	const FTimespan MediaDuration = MediaPlayer->GetDuration();

	if (MediaDuration.IsZero())
	{
		return LayerId;
	}

	TSharedRef<FMediaPlayerFacade, ESPMode::ThreadSafe> MediaPlayerFacade = MediaPlayer->GetPlayerFacade();

	InPainter.DrawElements.PushClip(ClippingZone);
	{
		TRangeSet<FTimespan> CacheRangeSet;

		MediaPlayerFacade->QueryCacheState(EMediaTrackType::Video, EMediaCacheState::Pending, CacheRangeSet);
		DrawSampleStates(InPainter, MediaDuration, SectionSize, CacheRangeSet, FLinearColor::Gray);

		CacheRangeSet.Empty();

		MediaPlayerFacade->QueryCacheState(EMediaTrackType::Video, EMediaCacheState::Loading, CacheRangeSet);
		DrawSampleStates(InPainter, MediaDuration, SectionSize, CacheRangeSet, FLinearColor::Yellow);

		CacheRangeSet.Empty();

		MediaPlayerFacade->QueryCacheState(EMediaTrackType::Video, EMediaCacheState::Loaded, CacheRangeSet);
		DrawSampleStates(InPainter, MediaDuration, SectionSize, CacheRangeSet, FLinearColor(0.10616, 0.48777, 0.10616));

		CacheRangeSet.Empty();

		MediaPlayerFacade->QueryCacheState(EMediaTrackType::Video, EMediaCacheState::Cached, CacheRangeSet);
		DrawSampleStates(InPainter, MediaDuration, SectionSize, CacheRangeSet, FLinearColor(0.07059, 0.32941, 0.07059));

		DrawLoopIndicators(InPainter, MediaDuration, SectionSize);

		DrawMediaInfo(InPainter, MediaPlayer, SectionSize);
	}
	InPainter.DrawElements.PopClip();

	return LayerId;
}


void FMediaThumbnailSection::SetSingleTime(double GlobalTime)
{
	UMovieSceneMediaSection* MediaSection = CastChecked<UMovieSceneMediaSection>(Section);

	if (MediaSection != nullptr)
	{
		double StartTime = MediaSection->GetInclusiveStartFrame() / MediaSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
		MediaSection->SetThumbnailReferenceOffset(GlobalTime - StartTime);
	}
}


void FMediaThumbnailSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	if (MediaSection != nullptr)
	{
		if (GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails)
		{
			ThumbnailCache.SetSingleReferenceFrame(MediaSection->GetThumbnailReferenceOffset());
		}
		else
		{
			ThumbnailCache.SetSingleReferenceFrame(TOptional<double>());
		}
	}

	FThumbnailSection::Tick(AllottedGeometry, ClippedGeometry, InCurrentTime, InDeltaTime);
}

void FMediaThumbnailSection::BeginResizeSection()
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);
	InitialStartOffsetDuringResize = MediaSection->StartFrameOffset;
	InitialStartTimeDuringResize = MediaSection->HasStartFrame() ? MediaSection->GetInclusiveStartFrame() : 0;
}

void FMediaThumbnailSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	if (ResizeMode == SSRM_LeadingEdge && MediaSection)
	{
		FFrameNumber StartOffset = ResizeTime - InitialStartTimeDuringResize;
		StartOffset += InitialStartOffsetDuringResize;

		// Ensure start offset is not less than 0
		if (StartOffset < 0)
		{
			ResizeTime = ResizeTime - StartOffset;

			StartOffset = FFrameNumber(0);
		}

		MediaSection->StartFrameOffset = StartOffset;
	}

	ISequencerSection::ResizeSection(ResizeMode, ResizeTime);
}

void FMediaThumbnailSection::BeginSlipSection()
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);
	InitialStartOffsetDuringResize = MediaSection->StartFrameOffset;
	InitialStartTimeDuringResize = MediaSection->HasStartFrame() ? MediaSection->GetInclusiveStartFrame() : 0;
}

void FMediaThumbnailSection::SlipSection(FFrameNumber SlipTime)
{
	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	const FFrameRate FrameRate = MediaSection->GetTypedOuter<UMovieScene>()->GetTickResolution();

	FFrameNumber StartOffset = SlipTime - InitialStartTimeDuringResize;
	StartOffset += InitialStartOffsetDuringResize;

	// Ensure start offset is not less than 0
	if (StartOffset < 0)
	{
		SlipTime = SlipTime - StartOffset;

		StartOffset = FFrameNumber(0);
	}

	MediaSection->StartFrameOffset = StartOffset;

	ISequencerSection::SlipSection(SlipTime);
}

/* ICustomThumbnailClient interface
 *****************************************************************************/

void FMediaThumbnailSection::Draw(FTrackEditorThumbnail& TrackEditorThumbnail)
{
}


void FMediaThumbnailSection::Setup()
{
}


/* FMediaThumbnailSection implementation
 *****************************************************************************/

void FMediaThumbnailSection::DrawFilmBorder(FSequencerSectionPainter& InPainter, FVector2D SectionSize) const
{
	static const FSlateBrush* FilmBorder = FAppStyle::GetBrush("Sequencer.Section.FilmBorder");

	// draw top film border
	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(SectionSize.X - 2.0f, 7.0f), FSlateLayoutTransform(FVector2D(1.0f, 4.0f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);

	// draw bottom film border
	FSlateDrawElement::MakeBox(
		InPainter.DrawElements,
		InPainter.LayerId++,
		InPainter.SectionGeometry.ToPaintGeometry(FVector2D(SectionSize.X - 2.0f, 7.0f), FSlateLayoutTransform(FVector2D(1.0f, SectionSize.Y - 11.0f))),
		FilmBorder,
		InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect
	);
}


void FMediaThumbnailSection::DrawLoopIndicators(FSequencerSectionPainter& InPainter, FTimespan MediaDuration, FVector2D SectionSize) const
{
	using namespace UE::Sequencer;

	static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");

	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
	double SectionDuration = FFrameTime(UE::MovieScene::DiscreteSize(Section->GetRange())) / TickResolution;
	const float MediaSizeX = MediaDuration.GetTotalSeconds() * SectionSize.X / SectionDuration;
	const FFrameNumber SectionOffset = MediaSection->GetRange().HasLowerBound() ? MediaSection->GetRange().GetLowerBoundValue() : 0;
	float DrawOffset = MediaSizeX - TimeToPixelConverter.SecondsToPixel(TickResolution.AsSeconds(SectionOffset + MediaSection->StartFrameOffset));

	while (DrawOffset < SectionSize.X)
	{
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(FVector2D(DrawOffset, 0.0f), FVector2D(1.0f, SectionSize.Y)),
			GenericBrush,
			ESlateDrawEffect::None,
			FLinearColor::Gray
		);

		DrawOffset += MediaSizeX;
	}
}


void FMediaThumbnailSection::DrawSampleStates(FSequencerSectionPainter& InPainter, FTimespan MediaDuration, FVector2D SectionSize, const TRangeSet<FTimespan>& RangeSet, const FLinearColor& Color) const
{
	using namespace UE::Sequencer;

	static const FSlateBrush* GenericBrush = FCoreStyle::Get().GetBrush("GenericWhiteBox");

	UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);

	const FTimeToPixel& TimeToPixelConverter = InPainter.GetTimeConverter();

	FFrameRate TickResolution = Section->GetTypedOuter<UMovieScene>()->GetTickResolution();
	double SectionDuration = FFrameTime(UE::MovieScene::DiscreteSize(Section->GetRange())) / TickResolution;
	const float MediaSizeX = MediaDuration.GetTotalSeconds() * SectionSize.X / SectionDuration;

	TArray<TRange<FTimespan>> Ranges;
	RangeSet.GetRanges(Ranges);

	for (auto& Range : Ranges)
	{
		const float DrawOffset = FMath::RoundToNegativeInfinity(FTimespan::Ratio(Range.GetLowerBoundValue(), MediaDuration) * MediaSizeX) -
			TimeToPixelConverter.SecondsToPixel(TickResolution.AsSeconds(MediaSection->StartFrameOffset)) +
			TimeToPixelConverter.SecondsToPixel(0.0);
		const float DrawSize = FMath::RoundToPositiveInfinity(FTimespan::Ratio(Range.Size<FTimespan>(), MediaDuration) * MediaSizeX);
		const float BarHeight = 4.0f;

		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(FVector2D(DrawOffset, SectionSize.Y - BarHeight - 1.0f), FVector2D(DrawSize, BarHeight)),
			GenericBrush,
			ESlateDrawEffect::None,
			Color
		);
	}
}


void FMediaThumbnailSection::DrawMediaInfo(FSequencerSectionPainter& InPainter, 
	UMediaPlayer* MediaPlayer,FVector2D SectionSize) const
{
	// Get tile info.
	FString TileString;
	FIntPoint TileNum(EForceInit::ForceInitToZero);
	MediaPlayer->GetMediaInfo<FIntPoint>(TileNum, UMediaPlayer::MediaInfoNameSourceNumTiles.Resolve());
	int32 TileTotalNum = TileNum.X * TileNum.Y;
	if (TileTotalNum > 1)
	{
		TileString = FText::Format(LOCTEXT("TileNum", "Tiles: {0}"), TileTotalNum).ToString();
	}

	// Get mip info.
	FString MipString;
	int32 MipNum = 0;
	MediaPlayer->GetMediaInfo<int32>(MipNum, UMediaPlayer::MediaInfoNameSourceNumMips.Resolve());
	if (MipNum > 1)
	{
		MipString = FText::Format(LOCTEXT("Mips", "Mips: {0}"), MipNum).ToString();
	}

	const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
	const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	
	FVector2D TextSize(EForceInit::ForceInitToZero);
	FMargin ContentPadding = GetContentPadding();
	FVector2D TextOffset(EForceInit::ForceInitToZero);
	float BaseYOffset = 0.0f;

	// Add tile string.
	if (TileString.Len() > 0)
	{
		TextSize = FontMeasureService->Measure(TileString, SmallLayoutFont);
		TextOffset.Set(ContentPadding.Left + 4,
			InPainter.SectionGeometry.Size.Y - (TextSize.Y + ContentPadding.Bottom));
		TextOffset.Y += BaseYOffset;
		FSlateDrawElement::MakeText(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(TextOffset, TextSize),
			TileString,
			SmallLayoutFont,
			DrawEffects,
			FColor(192, 192, 192, static_cast<uint8>(InPainter.GhostAlpha * 255))
		);
		BaseYOffset -= TextSize.Y + 2.0f;
	}

	// Add mips string.
	if (MipString.Len() > 0)
	{
		TextSize = FontMeasureService->Measure(MipString, SmallLayoutFont);
		TextOffset.Set(ContentPadding.Left + 4,
			InPainter.SectionGeometry.Size.Y - (TextSize.Y + ContentPadding.Bottom));
		TextOffset.Y += BaseYOffset;
		FSlateDrawElement::MakeText(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(TextOffset, TextSize),
			MipString,
			SmallLayoutFont,
			DrawEffects,
			FColor(192, 192, 192, static_cast<uint8>(InPainter.GhostAlpha * 255))
		);
		BaseYOffset -= TextSize.Y + 2.0f;
	}
}

UMediaPlayer* FMediaThumbnailSection::GetTemplateMediaPlayer() const
{
	// locate the track that evaluates this section
	if (!SectionPtr.IsValid())
	{
		return nullptr;
	}

	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();

	if (!Sequencer.IsValid())
	{
		return nullptr; // no movie scene player
	}

	// @todo: arodham: Test this and/or check dirty/compile?
	FMovieSceneRootEvaluationTemplateInstance& Instance = Sequencer->GetEvaluationTemplate();

	FMovieSceneSequenceID           SequenceId          = Sequencer->GetFocusedTemplateID();
	UMovieSceneCompiledDataManager* CompiledDataManager = Instance.GetCompiledDataManager();
	UMovieSceneSequence*            SubSequence         = Instance.GetSequence(SequenceId);
	FMovieSceneCompiledDataID       CompiledDataID      = CompiledDataManager->GetDataID(SubSequence);

	if (!CompiledDataID.IsValid())
	{
		return nullptr;
	}

	const FMovieSceneEvaluationTemplate* Template = CompiledDataManager->FindTrackTemplate(CompiledDataID);
	if (Template == nullptr)
	{
		return nullptr; // section template not found
	}

	auto OwnerTrack = Cast<UMovieSceneTrack>(SectionPtr->GetOuter());

	if (OwnerTrack == nullptr)
	{
		return nullptr; // media track not found
	}

	const FMovieSceneTrackIdentifier  TrackIdentifier = Template->GetLedger().FindTrackIdentifier(OwnerTrack->GetSignature());
	const FMovieSceneEvaluationTrack* EvaluationTrack = Template->FindTrack(TrackIdentifier);

	if (EvaluationTrack == nullptr)
	{
		return nullptr; // evaluation track not found
	}

	FMovieSceneMediaData* MediaData = nullptr;

	// find the persistent data of the section being drawn
	TArrayView<const FMovieSceneEvalTemplatePtr> Children = EvaluationTrack->GetChildTemplates();
	FPersistentEvaluationData PersistentData(*Sequencer.Get());

	for (int32 ChildIndex = 0; ChildIndex < Children.Num(); ++ChildIndex)
	{
		if (Children[ChildIndex]->GetSourceSection() == SectionPtr)
		{
			FMovieSceneEvaluationKey SectionKey(SequenceId, TrackIdentifier, ChildIndex);
			PersistentData.SetSectionKey(SectionKey);
			MediaData = PersistentData.FindSectionData<FMovieSceneMediaData>();

			break;
		}
	}

	// get the template's media player
	if (MediaData == nullptr)
	{
		return nullptr; // section persistent data not found
	}

	return MediaData->GetMediaPlayer();
}


#undef LOCTEXT_NAMESPACE
