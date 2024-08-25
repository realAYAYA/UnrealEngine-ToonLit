// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/SubTrackEditorBase.h"
#include "Fonts/FontCache.h"
#include "FrameNumberDisplayFormat.h"
#include "FrameNumberNumericInterface.h"
#include "MovieScene.h"
#include "LevelSequence.h"
#include "MovieSceneMetaData.h"
#include "SequencerSettings.h"
#include "Tracks/MovieSceneCinematicShotTrack.h"
#include "Tracks/MovieSceneSubTrack.h"

#define LOCTEXT_NAMESPACE "FSubTrackEditorBase"

FSubSectionPainterResult FSubSectionPainterUtil::PaintSection(TSharedPtr<const ISequencer> Sequencer, const UMovieSceneSubSection& SectionObject, FSequencerSectionPainter& InPainter, FSubSectionPainterParams Params)
{
    const TRange<FFrameNumber> SectionRange = SectionObject.GetRange();
    if (SectionRange.GetLowerBound().IsOpen() || SectionRange.GetUpperBound().IsOpen())
    {
        return FSSPR_InvalidSection;
    }

    const int32 SectionSize = UE::MovieScene::DiscreteSize(SectionRange);
    if (SectionSize <= 0)
    {
        return FSSPR_InvalidSection;
    }

    UMovieSceneSequence* InnerSequence = SectionObject.GetSequence();
    if (InnerSequence == nullptr || InnerSequence->GetMovieScene() == nullptr)
    {
        return FSSPR_NoInnerSequence;
    }

    const ESlateDrawEffect DrawEffects = InPainter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;
    if (SectionObject.Parameters.bCanLoop && !FMath::IsNearlyZero(SectionObject.OuterToInnerTransform().GetTimeScale()))
    {
        DoPaintLoopingSection(SectionObject, *InnerSequence, InPainter, DrawEffects);
    }
    else
    {
        DoPaintNonLoopingSection(SectionObject, *InnerSequence, InPainter, DrawEffects);
    }

    UMovieScene* MovieScene = InnerSequence->GetMovieScene();
    const int32 NumTracks = MovieScene->GetPossessableCount() + MovieScene->GetSpawnableCount() + MovieScene->GetTracks().Num();

    FVector2D TopLeft = InPainter.SectionGeometry.AbsoluteToLocal(InPainter.SectionClippingRect.GetTopLeft()) + FVector2D(1.f, -1.f);

    FSlateFontInfo FontInfo = FAppStyle::GetFontStyle("NormalFont");

    TSharedRef<FSlateFontCache> FontCache = FSlateApplication::Get().GetRenderer()->GetFontCache();

    auto GetFontHeight = [&]
    {
        return FontCache->GetMaxCharacterHeight(FontInfo, 1.f) + FontCache->GetBaseline(FontInfo, 1.f);
    };
    while (GetFontHeight() > InPainter.SectionGeometry.Size.Y && FontInfo.Size > 11)
    {
        FontInfo.Size = FMath::Max(FMath::FloorToInt(FontInfo.Size - 6.f), 11);
    }

    uint32 LayerId = InPainter.LayerId;
    FMargin ContentPadding = Params.ContentPadding;

    FText TrackNumText, NetworkText;
    if (Params.bShowTrackNum)
    {
        TrackNumText = FText::Format(LOCTEXT("NumTracksFormat", "{0} track(s)"), FText::AsNumber(NumTracks));
    }

    EMovieSceneServerClientMask NetworkMask = SectionObject.GetNetworkMask();
    if (NetworkMask == EMovieSceneServerClientMask::Client)
    {
        NetworkText = LOCTEXT("SubSectionClientOnlyText", "(client only)");
    }
    else if (NetworkMask == EMovieSceneServerClientMask::Server)
    {
        NetworkText = LOCTEXT("SubSectionServerOnlyText", "(server only)");
    }

    FText SectionText;
    if (!TrackNumText.IsEmpty() || !NetworkText.IsEmpty())
    {
        SectionText = FText::Format(LOCTEXT("SectionTextFormat", "{0} {1}"), TrackNumText, NetworkText);
    }
    else if (!TrackNumText.IsEmpty())
    {
        SectionText = TrackNumText;
    }
    else
    {
        SectionText = NetworkText;
    }


    if (!SectionText.IsEmpty())
    {
		FSlateClippingZone ClippingZone(InPainter.SectionClippingRect.InsetBy(FMargin(1.0f)));

		InPainter.DrawElements.PushClip(ClippingZone);
		
		FSlateDrawElement::MakeText(
            InPainter.DrawElements,
            ++LayerId,
            InPainter.SectionGeometry.ToPaintGeometry(
                FVector2D(InPainter.SectionGeometry.Size.X, GetFontHeight()),
                FSlateLayoutTransform(TopLeft + FVector2D(ContentPadding.Left, ContentPadding.Top) + FVector2D(11.f, GetFontHeight()*2.f))
            ),
            SectionText,
            FontInfo,
            DrawEffects,
            FColor(200, 200, 200, static_cast<uint8>(255 * InPainter.GhostAlpha))
        );

		InPainter.DrawElements.PopClip();
	}

    InPainter.LayerId = LayerId;

    return FSSPR_Success;
}

void FSubSectionPainterUtil::DoPaintNonLoopingSection(const UMovieSceneSubSection& SectionObject, const UMovieSceneSequence& InnerSequence, FSequencerSectionPainter& InPainter, ESlateDrawEffect DrawEffects)
{
    const uint8 GhostAlpha = static_cast<uint8>(255 * InPainter.GhostAlpha);

    const FFrameNumber SectionStartFrame = SectionObject.GetInclusiveStartFrame();

    const TRange<FFrameNumber> SectionRange = SectionObject.GetRange();
    const int32 SectionSize = UE::MovieScene::DiscreteSize(SectionRange);
    const float PixelsPerFrame = InPainter.SectionGeometry.Size.X / float(SectionSize);

    UMovieScene* MovieScene = InnerSequence.GetMovieScene();
    TRange<FFrameNumber> PlaybackRange = MovieScene->GetPlaybackRange();

    // We're in the non-looping case so we know we have a purely linear transform (unless zero timescale)
	const FMovieSceneSequenceTransform OuterToInnerTransform = SectionObject.OuterToInnerTransform();
    const FMovieSceneSequenceTransform InnerToOuterTransform = OuterToInnerTransform.InverseNoLooping();
    const FFrameNumber PlaybackStart = (UE::MovieScene::DiscreteInclusiveLower(PlaybackRange) * InnerToOuterTransform).FloorToFrame();
	const FFrameNumber PlaybackEnd = (UE::MovieScene::DiscreteExclusiveUpper(PlaybackRange) * InnerToOuterTransform).FloorToFrame();
	const TRange<FFrameNumber> PlaybackRangeTransformed(PlaybackStart, PlaybackEnd);
	bool bZeroTimescale = !FMath::IsFinite(InnerToOuterTransform.GetTimeScale());

	// Section contains the beginning but not the end- draw beginning, draw dark tint before it
	// Section contains the end but not the beginning- draw dark tint, draw dark tint after it
	// Section contains both end and beginning- draw both, dark tint before beginning, dark tint after end
	// Section contains neither or zero-timescale and the frame isn't in the section range- draw dark tint over entire section
	// Zero-timescale and the frame is in the sectionrange- don't draw anything extra
	const int32 StartOffset = (PlaybackStart - SectionStartFrame).Value;
	const int32 EndOffset = (PlaybackEnd - SectionStartFrame).Value;

	if (bZeroTimescale)
	{
		// If we're zero-timescale we can't use the inverse transform to determine bounds, as the transformation is non-deterministic.
		// Instead, we just ask whether the playback start is in the interior range, and if it doesn't, draw dark tint over the entire section
		
		const FFrameTime InnerFrame = SectionStartFrame * OuterToInnerTransform;
		if (!PlaybackRange.Contains(InnerFrame.FloorToFrame()))
		{
			// add dark tint over the entire section
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				InPainter.LayerId++,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2f(InPainter.SectionGeometry.Size.X, InPainter.SectionGeometry.Size.Y),
					FSlateLayoutTransform()
				),
				FAppStyle::GetBrush("WhiteBrush"),
				DrawEffects,
				FLinearColor::Black.CopyWithNewOpacity(0.5f)
			);
		}
	}
	else if (TRange<FFrameNumber>::Intersection(PlaybackRangeTransformed, SectionRange).IsEmpty())
	{
		// add dark tint over the entire section
		FSlateDrawElement::MakeBox(
			InPainter.DrawElements,
			InPainter.LayerId++,
			InPainter.SectionGeometry.ToPaintGeometry(
				FVector2f(InPainter.SectionGeometry.Size.X, InPainter.SectionGeometry.Size.Y),
				FSlateLayoutTransform()
			),
			FAppStyle::GetBrush("WhiteBrush"),
			DrawEffects,
			FLinearColor::Black.CopyWithNewOpacity(0.5f)
		);
	}
	else
	{
		if (SectionRange.Contains(PlaybackStart))
		{
			// add dark tint for left out-of-bounds
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				InPainter.LayerId++,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2f(StartOffset * PixelsPerFrame, InPainter.SectionGeometry.Size.Y),
					FSlateLayoutTransform()
				),
				FAppStyle::GetBrush("WhiteBrush"),
				DrawEffects,
				FLinearColor::Black.CopyWithNewOpacity(0.5f)
			);

			// add green line for playback start
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				InPainter.LayerId++,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2f(1.0f, InPainter.SectionGeometry.Size.Y),
					FSlateLayoutTransform(FVector2f(StartOffset * PixelsPerFrame, 0.f))
				),
				FAppStyle::GetBrush("WhiteBrush"),
				DrawEffects,
				FColor(32, 128, 32, GhostAlpha)	// 120, 75, 50 (HSV)
			);
		}

		if (SectionRange.Contains(PlaybackEnd))
		{
			// add dark tint for right out-of-bounds
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				InPainter.LayerId++,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2f((SectionSize - EndOffset) * PixelsPerFrame, InPainter.SectionGeometry.Size.Y),
					FSlateLayoutTransform(FVector2f(EndOffset * PixelsPerFrame, 0.f))
				),
				FAppStyle::GetBrush("WhiteBrush"),
				DrawEffects,
				FLinearColor::Black.CopyWithNewOpacity(0.5f)
			);


			// add red line for playback end
			FSlateDrawElement::MakeBox(
				InPainter.DrawElements,
				InPainter.LayerId++,
				InPainter.SectionGeometry.ToPaintGeometry(
					FVector2f(1.0f, InPainter.SectionGeometry.Size.Y),
					FSlateLayoutTransform(FVector2f(EndOffset * PixelsPerFrame, 0.f))
				),
				FAppStyle::GetBrush("WhiteBrush"),
				DrawEffects,
				FColor(128, 32, 32, GhostAlpha)	// 0, 75, 50 (HSV)
			);
		}
	}
}

void FSubSectionPainterUtil::DoPaintLoopingSection(const UMovieSceneSubSection& SectionObject, const UMovieSceneSequence& InnerSequence, FSequencerSectionPainter& InPainter, ESlateDrawEffect DrawEffects)
{
    const TRange<FFrameNumber> SectionRange = SectionObject.GetRange();
    const int32 SectionSize = UE::MovieScene::DiscreteSize(SectionRange);
    const float PixelsPerFrame = InPainter.SectionGeometry.Size.X / float(SectionSize);

    const float InvTimeScale = FMath::IsNearlyZero(SectionObject.Parameters.TimeScale) ? 1.0f : 1.0f / SectionObject.Parameters.TimeScale;

    const UMovieScene* MovieScene = InnerSequence.GetMovieScene();
    const TRange<FFrameNumber> InnerPlaybackRange = UMovieSceneSubSection::GetValidatedInnerPlaybackRange(SectionObject.Parameters, *MovieScene);

    const FFrameNumber InnerSubSeqLength = UE::MovieScene::DiscreteSize(InnerPlaybackRange);
    const FFrameNumber InnerSubSeqFirstLoopLength = InnerSubSeqLength - SectionObject.Parameters.FirstLoopStartFrameOffset;

    const FFrameRate OuterFrameRate   = SectionObject.GetTypedOuter<UMovieScene>()->GetTickResolution();
    const FFrameRate InnerFrameRate   = MovieScene->GetTickResolution();
    const FFrameNumber OuterSubSeqLength = (ConvertFrameTime(InnerSubSeqLength, InnerFrameRate, OuterFrameRate) * InvTimeScale).FrameNumber;
    const FFrameNumber OuterSubSeqFirstLoopLength = (ConvertFrameTime(InnerSubSeqFirstLoopLength, InnerFrameRate, OuterFrameRate) * InvTimeScale).FrameNumber;

    FFrameNumber CurOffsetFrame = FMath::Max(OuterSubSeqFirstLoopLength, FFrameNumber(0));

    const uint8 GhostAlpha = static_cast<uint8>(255 * InPainter.GhostAlpha);

    // Draw separators where the sub-sequence is looping. To be consistent with the non-looping case, we draw a red and green
    // separator back to back.
    uint32 MaxLoopBoundaries = 100;
    while (CurOffsetFrame < SectionSize)
    {
        const int32 CurOffset = CurOffsetFrame.Value;
        FSlateDrawElement::MakeBox(
            InPainter.DrawElements,
            InPainter.LayerId++,
            InPainter.SectionGeometry.ToPaintGeometry(
                FVector2f(1.0f, InPainter.SectionGeometry.Size.Y),
                FSlateLayoutTransform(FVector2f(CurOffset * PixelsPerFrame, 0.f))
            ),
            FAppStyle::GetBrush("WhiteBrush"),
            DrawEffects,
            FColor(32, 128, 32, GhostAlpha)	// 120, 75, 50 (HSV)
        );
        if (CurOffset > 0)
        {
            FSlateDrawElement::MakeBox(
                InPainter.DrawElements,
                InPainter.LayerId++,
                InPainter.SectionGeometry.ToPaintGeometry(
                    FVector2f(1.0f, InPainter.SectionGeometry.Size.Y),
                    FSlateLayoutTransform(FVector2f(CurOffset * PixelsPerFrame - 1.f, 0.f))
                ),
                FAppStyle::GetBrush("WhiteBrush"),
                DrawEffects,
                FColor(128, 32, 32, GhostAlpha)	// 0, 75, 50 (HSV)
            );
        }

        CurOffsetFrame += OuterSubSeqLength;

        if ((--MaxLoopBoundaries) == 0)
        {
            break;
        }
    }
}

FSubSectionEditorUtil::FSubSectionEditorUtil(UMovieSceneSubSection& InSection)
    : SectionObject(InSection)
    , InitialStartOffsetDuringResize(0)
    , InitialStartTimeDuringResize(0)
	, PreviousTimeScale(1.f)
{
}

void FSubSectionEditorUtil::BeginResizeSection()
{
    InitialStartOffsetDuringResize = SectionObject.Parameters.bCanLoop ? SectionObject.Parameters.FirstLoopStartFrameOffset : SectionObject.Parameters.StartFrameOffset;
    InitialStartTimeDuringResize = SectionObject.HasStartFrame() ? SectionObject.GetInclusiveStartFrame() : 0;
}

FFrameNumber FSubSectionEditorUtil::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
{
    UMovieSceneSequence* InnerSequence = SectionObject.GetSequence();
    UMovieScene* InnerMovieScene = InnerSequence ? InnerSequence->GetMovieScene() : nullptr;

    if (ResizeMode == SSRM_LeadingEdge && InnerMovieScene != nullptr)
    {
        // Adjust the start offset when resizing from the beginning
        FMovieSceneSectionParameters& SectionParameters = SectionObject.Parameters;

        const FFrameRate    OuterFrameRate   = SectionObject.GetTypedOuter<UMovieScene>()->GetTickResolution();
        const FFrameRate    InnerFrameRate   = InnerSequence->GetMovieScene()->GetTickResolution();
        const FFrameNumber  ResizeDifference = ResizeTime - InitialStartTimeDuringResize;

        const FFrameNumber InnerResizeDifference = (ConvertFrameTime(ResizeDifference, OuterFrameRate, InnerFrameRate) * SectionParameters.TimeScale).FrameNumber;
        FFrameNumber NewStartOffset = InitialStartOffsetDuringResize + InnerResizeDifference;

        const int32 InnerPlaybackLength = UE::MovieScene::DiscreteSize(InnerMovieScene->GetPlaybackRange());
        const float InvTimeScale = FMath::IsNearlyZero(SectionParameters.TimeScale) ? 1.0f : 1.0f / SectionParameters.TimeScale;
        const FFrameNumber InnerLoopLength = InnerPlaybackLength - SectionParameters.StartFrameOffset - SectionParameters.EndFrameOffset;

        const bool bCanLoop = SectionParameters.bCanLoop;
        if (!bCanLoop)
        {
            if (NewStartOffset < 0)
            {
                // Ensure start offset is not less than 0, clamp resize time.
                const FFrameTime OuterFrameTimeOver = ConvertFrameTime(FFrameTime::FromDecimal(NewStartOffset.Value / SectionParameters.TimeScale), InnerFrameRate, OuterFrameRate);
                ResizeTime = ResizeTime - OuterFrameTimeOver.GetFrame(); 
                NewStartOffset = 0;
            }
            SectionParameters.StartFrameOffset = NewStartOffset;
        }
        else
        {
			// If the start offset exceeds the length of one loop, trim it back.
			NewStartOffset = NewStartOffset % InnerLoopLength;
            if (NewStartOffset < 0)
            {
                // Move the first loop offset forward so that it's positive into the new first loop
                // that we just "revealed" by pulling the left edge back.
                NewStartOffset += InnerLoopLength;
            }
            SectionParameters.FirstLoopStartFrameOffset = NewStartOffset;
        }
    }

    return ResizeTime;
}

void FSubSectionEditorUtil::BeginSlipSection()
{
    // Cache the same values as when resizing.
    BeginResizeSection();
}

FFrameNumber FSubSectionEditorUtil::SlipSection(FFrameNumber SlipTime)
{
    UMovieSceneSequence* InnerSequence = SectionObject.GetSequence();
    UMovieScene* InnerMovieScene = InnerSequence ? InnerSequence->GetMovieScene() : nullptr;

    if (InnerMovieScene != nullptr)
    {
        FMovieSceneSectionParameters& SectionParameters = SectionObject.Parameters;

        const FFrameRate    OuterFrameRate   = SectionObject.GetTypedOuter<UMovieScene>()->GetTickResolution();
        const FFrameRate    InnerFrameRate   = InnerSequence->GetMovieScene()->GetTickResolution();
        const FFrameNumber  ResizeDifference = SlipTime - InitialStartTimeDuringResize;

        const FFrameNumber InnerResizeDifference = (ConvertFrameTime(ResizeDifference, OuterFrameRate, InnerFrameRate) * SectionParameters.TimeScale).FrameNumber;
        FFrameNumber NewStartOffset = InitialStartOffsetDuringResize + InnerResizeDifference;

        const int32 InnerPlaybackLength = UE::MovieScene::DiscreteSize(InnerMovieScene->GetPlaybackRange());
        const float InvTimeScale = FMath::IsNearlyZero(SectionParameters.TimeScale) ? 1.0f : 1.0f / SectionParameters.TimeScale;
        const FFrameNumber InnerLoopLength = InnerPlaybackLength - SectionParameters.StartFrameOffset - SectionParameters.EndFrameOffset;

        const bool bCanLoop = SectionParameters.bCanLoop;
        if (!bCanLoop)
        {
            // Ensure start offset is not less than 0
            SectionParameters.StartFrameOffset = FFrameNumber(FMath::Max(NewStartOffset.Value, 0));
        }
        else
        {
			// If the start offset exceeds the length of one loop, trim it back.
            NewStartOffset = NewStartOffset % InnerLoopLength;
            if (NewStartOffset < 0)
            {
                // Move the first loop offset forward so that it's positive into the new first loop
                // that we just "revealed" by slipping too much to the side.
                NewStartOffset += InnerLoopLength;
            }
            SectionParameters.FirstLoopStartFrameOffset = NewStartOffset;
        }
    }

    return SlipTime;
}

void FSubSectionEditorUtil::BeginDilateSection()
{
    FMovieSceneSectionParameters& SectionParameters = SectionObject.Parameters;
	PreviousTimeScale = SectionParameters.TimeScale;
}

void FSubSectionEditorUtil::DilateSection(const TRange<FFrameNumber>& NewRange, float DilationFactor)
{
    FMovieSceneSectionParameters& SectionParameters = SectionObject.Parameters;
	SectionParameters.TimeScale = PreviousTimeScale / DilationFactor;
	SectionObject.SetRange(NewRange);
}


bool FSubTrackEditorUtil::CanAddSubSequence(const UMovieSceneSequence* CurrentSequence, const UMovieSceneSequence& SubSequence)
{
	// Prevent adding ourselves and ensure we have a valid movie scene.
	if ((CurrentSequence == nullptr) || (CurrentSequence == &SubSequence) || (CurrentSequence->GetMovieScene() == nullptr))
	{
		return false;
	}

	// ensure that the other sequence has a valid movie scene
	UMovieScene* SequenceMovieScene = SubSequence.GetMovieScene();

	if (SequenceMovieScene == nullptr)
	{
		return false;
	}

	// make sure we are not contained in the other sequence (circular dependency)
	// @todo sequencer: this check is not sufficient (does not prevent circular dependencies of 2+ levels)
	UMovieSceneSubTrack* SequenceSubTrack = SequenceMovieScene->FindTrack<UMovieSceneSubTrack>();
	if (SequenceSubTrack && SequenceSubTrack->ContainsSequence(*CurrentSequence, true))
	{
		return false;
	}

	UMovieSceneCinematicShotTrack* SequenceCinematicTrack = SequenceMovieScene->FindTrack<UMovieSceneCinematicShotTrack>();
	if (SequenceCinematicTrack && SequenceCinematicTrack->ContainsSequence(*CurrentSequence, true))
	{
		return false;
	}

	return true;
}

UMovieSceneMetaData* FSubTrackEditorUtil::FindOrAddMetaData(UMovieSceneSequence* Sequence)
{
	if (!Sequence)
	{
		return nullptr;
	}

	ULevelSequence* LevelSequence = Cast<ULevelSequence>(Sequence);
	return LevelSequence ? LevelSequence->FindOrAddMetaData<UMovieSceneMetaData>() : nullptr;
}

FText FSubTrackEditorUtil::GetMetaDataText(const UMovieSceneSequence* Sequence)
{
	const ULevelSequence* LevelSequence = Cast<const ULevelSequence>(Sequence);
	if (!LevelSequence)
	{
		return FText::GetEmpty();
	}

	const UMovieSceneMetaData* MetaData = LevelSequence->FindMetaData<const UMovieSceneMetaData>();
	if (!MetaData)
	{
		return FText::GetEmpty();
	}

	if (MetaData->IsEmpty())
	{
		return FText::GetEmpty();
	}

	return FText::Format(LOCTEXT("MetaDataContentFormat", "Author: {0}\nCreated: {1}\nNotes: {2}"),
		FText::FromString(MetaData->GetAuthor()),
		FText::AsDateTime(MetaData->GetCreated()),
		FText::FromString(MetaData->GetNotes())
	);
}

#undef LOCTEXT_NAMESPACE
