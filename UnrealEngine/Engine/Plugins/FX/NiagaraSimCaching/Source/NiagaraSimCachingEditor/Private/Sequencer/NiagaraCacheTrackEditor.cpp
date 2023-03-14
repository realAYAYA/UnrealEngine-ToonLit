// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/NiagaraCacheTrackEditor.h"
#include "CommonMovieSceneTools.h"
#include "Fonts/FontMeasure.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "LevelSequence.h"
#include "NiagaraComponent.h"
#include "NiagaraSimCache.h"
#include "Niagara/Sequencer/MovieSceneNiagaraCacheTrack.h"
#include "Niagara/Sequencer/MovieSceneNiagaraCacheSection.h"
#include "SequencerSectionPainter.h"
#include "SequencerUtilities.h"
#include "Styling/SlateIconFinder.h"
#include "TimeToPixel.h"

namespace NiagaraCacheEditorConstants
{
	constexpr float AnimationTrackHeight = 20.f;
}

#define LOCTEXT_NAMESPACE "FNiagaraCacheTrackEditor"

static UNiagaraComponent* AcquireNiagaraComponentFromObjectGuid(const FGuid& Guid, TSharedPtr<ISequencer> SequencerPtr)
{
	UObject* BoundObject = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(Guid) : nullptr;
	return Cast<UNiagaraComponent>(BoundObject);
}

FNiagaraCacheSection::FNiagaraCacheSection(UMovieSceneSection& InSection, TWeakPtr<ISequencer> InSequencer)
	: Section(*CastChecked<UMovieSceneNiagaraCacheSection>(&InSection))
	, Sequencer(InSequencer)
	, InitialFirstLoopStartOffsetDuringResize(0)
	, InitialStartTimeDuringResize(0)
{ }

UMovieSceneSection* FNiagaraCacheSection::GetSectionObject()
{
	return &Section;
}

FText FNiagaraCacheSection::GetSectionTitle() const
{
	if (Section.Params.SimCache != nullptr)
	{
		FNumberFormattingOptions FormatOptions;
		FormatOptions.MaximumFractionalDigits = 1;
		return FText::Format(FText::FromString("Sim Cache ({0} frames/{1}s)"), Section.Params.SimCache->GetNumFrames(), FText::AsNumber(Section.Params.SimCache->GetDurationSeconds(), &FormatOptions));
	
	}
	return LOCTEXT("NoNiagaraCacheSection", "No NiagaraCache");
}

float FNiagaraCacheSection::GetSectionHeight() const
{
	return NiagaraCacheEditorConstants::AnimationTrackHeight;
}

int32 FNiagaraCacheSection::OnPaintSection(FSequencerSectionPainter& Painter) const
{
	using namespace UE::Sequencer;

	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	const FTimeToPixel& TimeToPixelConverter = Painter.GetTimeConverter();

	int32 LayerId = Painter.PaintSectionBackground();

	static const FSlateBrush* GenericDivider = FAppStyle::GetBrush("Sequencer.GenericDivider");

	if (!Section.HasStartFrame() || !Section.HasEndFrame())
	{
		return LayerId;
	}

	FFrameRate TickResolution = TimeToPixelConverter.GetTickResolution();

	// Add lines where the animation starts and ends/loops
	const float AnimPlayRate = FMath::IsNearlyZero(Section.Params.PlayRate) ? 1.0f : Section.Params.PlayRate;
	const float Duration = Section.Params.GetSequenceLength();
	const float SeqLength = Duration - TickResolution.AsSeconds(Section.Params.StartFrameOffset + Section.Params.EndFrameOffset) / AnimPlayRate;
	const float FirstLoopSeqLength = SeqLength - TickResolution.AsSeconds(Section.Params.FirstLoopStartFrameOffset) / AnimPlayRate;

	if (!FMath::IsNearlyZero(SeqLength, KINDA_SMALL_NUMBER) && SeqLength > 0)
	{
		float MaxOffset = Section.GetRange().Size<FFrameTime>() / TickResolution;
		float OffsetTime = FirstLoopSeqLength;
		float StartTime = Section.GetInclusiveStartFrame() / TickResolution;

		while (OffsetTime < MaxOffset)
		{
			float OffsetPixel = TimeToPixelConverter.SecondsToPixel(StartTime + OffsetTime) - TimeToPixelConverter.SecondsToPixel(StartTime);

			FSlateDrawElement::MakeBox(
				Painter.DrawElements,
				LayerId,
				Painter.SectionGeometry.MakeChild(
					FVector2D(2.f, Painter.SectionGeometry.Size.Y - 2.f),
					FSlateLayoutTransform(FVector2D(OffsetPixel, 1.f))
				).ToPaintGeometry(),
				GenericDivider,
				DrawEffects
			);

			OffsetTime += SeqLength;
		}
	}

	TSharedPtr<ISequencer> SequencerPtr = Sequencer.Pin();
	if (Painter.bIsSelected && SequencerPtr.IsValid())
	{
		FFrameTime CurrentTime = SequencerPtr->GetLocalTime().Time;
		if (Section.GetRange().Contains(CurrentTime.FrameNumber) && Section.Params.SimCache != nullptr)
		{
			const float Time = TimeToPixelConverter.FrameToPixel(CurrentTime);

			UNiagaraSimCache* NiagaraCache = Section.Params.SimCache;
			int32 NumFrames = NiagaraCache ? NiagaraCache->GetNumFrames() : 0;

			// Draw the current time next to the scrub handle
			const float AnimTime = Section.MapTimeToAnimation(Duration, CurrentTime, TickResolution);
			int32 FrameTime = Duration ? FMath::FloorToFloat((AnimTime / Duration) * NumFrames) + 1 : 0;
			FString FrameString =  FString::FromInt(FrameTime);

			const FSlateFontInfo SmallLayoutFont = FCoreStyle::GetDefaultFontStyle("Bold", 10);
			const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			FVector2D TextSize = FontMeasureService->Measure(FrameString, SmallLayoutFont);

			// Flip the text position if getting near the end of the view range
			static constexpr float TextOffsetPx = 10.f;
			bool  bDrawLeft = (Painter.SectionGeometry.Size.X - Time) < (TextSize.X + 22.f) - TextOffsetPx;
			float TextPosition = bDrawLeft ? Time - TextSize.X - TextOffsetPx : Time + TextOffsetPx;
			//handle mirrored labels
			constexpr float MajorTickHeight = 7.0f;
			FVector2D TextOffset(TextPosition, Painter.SectionGeometry.Size.Y - (MajorTickHeight + TextSize.Y));

			const FLinearColor DrawColor = FAppStyle::GetSlateColor("SelectionColor").GetColor(FWidgetStyle());
			const FVector2D BoxPadding = FVector2D(4.0f, 2.0f);
			// draw time string

			FSlateDrawElement::MakeBox(
				Painter.DrawElements,
				LayerId + 5,
				Painter.SectionGeometry.ToPaintGeometry(TextOffset - BoxPadding, TextSize + 2.0f * BoxPadding),
				FAppStyle::GetBrush("WhiteBrush"),
				ESlateDrawEffect::None,
				FLinearColor::Black.CopyWithNewOpacity(0.5f)
			);

			FSlateDrawElement::MakeText(
				Painter.DrawElements,
				LayerId + 6,
				Painter.SectionGeometry.ToPaintGeometry(TextOffset, TextSize),
				FrameString,
				SmallLayoutFont,
				DrawEffects,
				DrawColor
			);

		}
	}

	return LayerId;
}

void FNiagaraCacheSection::BeginResizeSection()
{
	InitialFirstLoopStartOffsetDuringResize = Section.Params.FirstLoopStartFrameOffset;
	InitialStartTimeDuringResize = Section.HasStartFrame() ? Section.GetInclusiveStartFrame() : 0;
}

void FNiagaraCacheSection::UpdateSection(FFrameNumber& UpdateTime) const
{
	const FFrameRate FrameRate = Section.GetTypedOuter<UMovieScene>()->GetTickResolution();
	FFrameNumber StartOffset = FrameRate.AsFrameNumber((UpdateTime - InitialStartTimeDuringResize) / FrameRate * Section.Params.PlayRate);

	StartOffset += InitialFirstLoopStartOffsetDuringResize;

	if (StartOffset < 0)
	{
		// Ensure start offset is not less than 0 and adjust ResizeTime
		UpdateTime = UpdateTime - StartOffset;

		StartOffset = FFrameNumber(0);
	}
	else
	{
		// If the start offset exceeds the length of one loop, trim it back.
		const FFrameNumber SeqLength = FrameRate.AsFrameNumber(Section.Params.GetSequenceLength()) - Section.Params.StartFrameOffset - Section.Params.EndFrameOffset;
		StartOffset = StartOffset % SeqLength;
	}

	Section.Params.FirstLoopStartFrameOffset = StartOffset;
}

void FNiagaraCacheSection::ResizeSection(ESequencerSectionResizeMode ResizeMode, FFrameNumber ResizeTime)
{
	// Adjust the start offset when resizing from the beginning
	if (ResizeMode == SSRM_LeadingEdge)
	{
		UpdateSection(ResizeTime);
	}

	ISequencerSection::ResizeSection(ResizeMode, ResizeTime);
}

void FNiagaraCacheSection::BeginSlipSection()
{
	BeginResizeSection();
}

void FNiagaraCacheSection::SlipSection(FFrameNumber SlipTime)
{
	UpdateSection(SlipTime);

	ISequencerSection::SlipSection(SlipTime);
}

void FNiagaraCacheSection::BeginDilateSection()
{
	Section.PreviousPlayRate = Section.Params.PlayRate; //make sure to cache the play rate
}
void FNiagaraCacheSection::DilateSection(const TRange<FFrameNumber>& NewRange, float DilationFactor)
{
	Section.Params.PlayRate = Section.PreviousPlayRate / DilationFactor;
	Section.SetRange(NewRange);
}

FNiagaraCacheTrackEditor::FNiagaraCacheTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
{ }

TSharedRef<ISequencerTrackEditor> FNiagaraCacheTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FNiagaraCacheTrackEditor(InSequencer));
}

bool FNiagaraCacheTrackEditor::SupportsSequence(UMovieSceneSequence* InSequence) const
{
	if (InSequence && InSequence->IsTrackSupported(UMovieSceneNiagaraCacheTrack::StaticClass()) == ETrackSupport::NotSupported)
	{
		return false;
	}

	return InSequence && InSequence->IsA(ULevelSequence::StaticClass());
}

bool FNiagaraCacheTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> Type) const
{
	return Type == UMovieSceneNiagaraCacheTrack::StaticClass();
}

TSharedRef<ISequencerSection> FNiagaraCacheTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	check(SupportsType(SectionObject.GetOuter()->GetClass()));

	return MakeShareable(new FNiagaraCacheSection(SectionObject, GetSequencer()));
}

void FNiagaraCacheTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	//TODO (mga) allow users to create cache tracks directly?
	/*
	if (ObjectClass->IsChildOf(UNiagaraComponent::StaticClass()) || ObjectClass->IsChildOf(AActor::StaticClass()))
	{
		if(ObjectBindings.Num() > 0)
		{
			if (UNiagaraComponent* NiagaraComponent = AcquireNiagaraComponentFromObjectGuid(ObjectBindings[0], GetSequencer()))
			{
				UMovieSceneTrack* Track = nullptr;

				
				MenuBuilder.AddMenuEntry(
					NSLOCTEXT("Sequencer", "AddNiagaraCache", "Niagara Cache"),
					NSLOCTEXT("Sequencer", "AddNiagaraCacheTooltip", "Adds a Niagara Cache track."),
					FSlateIcon(),
					FUIAction(
						FExecuteAction::CreateSP(this, &FNiagaraCacheTrackEditor::BuildNiagaraCacheTrack, ObjectBindings, Track)
					)
				);
				
			}
		}
	}*/
}

void FNiagaraCacheTrackEditor::BuildNiagaraCacheTrack(TArray<FGuid> ObjectBindings, UMovieSceneTrack* Track)
{
	const TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

	if (SequencerPtr.IsValid())
	{
		const FScopedTransaction Transaction(LOCTEXT("AddNiagaraCache_Transaction", "Add Niagara Cache"));

		for (FGuid ObjectBinding : ObjectBindings)
		{
			if (UNiagaraComponent* NiagaraComponent = AcquireNiagaraComponentFromObjectGuid(ObjectBinding, SequencerPtr))
				{
					AnimatablePropertyChanged(FOnKeyProperty::CreateRaw(this, &FNiagaraCacheTrackEditor::AddKeyInternal, NiagaraComponent, Track));
				}
		}
	}
}

FKeyPropertyResult FNiagaraCacheTrackEditor::AddKeyInternal(FFrameNumber KeyTime, UNiagaraComponent* NiagaraComponent, UMovieSceneTrack* Track)
{
	FKeyPropertyResult KeyPropertyResult;

	const FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(NiagaraComponent);
	const FGuid ObjectHandle = HandleResult.Handle;
	KeyPropertyResult.bHandleCreated |= HandleResult.bWasCreated;
	if (ObjectHandle.IsValid())
	{
		if (!Track)
		{
			Track = AddTrack(GetSequencer()->GetFocusedMovieSceneSequence()->GetMovieScene(), ObjectHandle, UMovieSceneNiagaraCacheTrack::StaticClass(), NAME_None);
			KeyPropertyResult.bTrackCreated = true;
		}

		if (ensure(Track))
		{
			Track->Modify();

			UMovieSceneSection* NewSection = Cast<UMovieSceneNiagaraCacheTrack>(Track)->AddNewAnimation(KeyTime, NiagaraComponent);
			KeyPropertyResult.bTrackModified = true;
			KeyPropertyResult.SectionsCreated.Add(NewSection);

			GetSequencer()->EmptySelection();
			GetSequencer()->SelectSection(NewSection);
			GetSequencer()->ThrobSectionSelection();
		}
	}

	return KeyPropertyResult;
}

TSharedPtr<SWidget> FNiagaraCacheTrackEditor::BuildOutlinerEditWidget(const FGuid& ObjectBinding, UMovieSceneTrack* Track, const FBuildEditWidgetParams& Params)
{
	return TSharedPtr<SWidget>();
}

const FSlateBrush* FNiagaraCacheTrackEditor::GetIconBrush() const
{
	return FSlateIconFinder::FindIconForClass(UNiagaraSimCache::StaticClass()).GetIcon();
}

#undef LOCTEXT_NAMESPACE
