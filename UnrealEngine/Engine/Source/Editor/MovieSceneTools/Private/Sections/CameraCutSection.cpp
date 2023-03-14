// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sections/CameraCutSection.h"
#include "Sections/MovieSceneCameraCutSection.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "MovieScene.h"
#include "SequencerSectionPainter.h"
#include "ScopedTransaction.h"
#include "MovieSceneSequence.h"
#include "MovieSceneCommonHelpers.h"
#include "Evaluation/MovieSceneEvaluationTemplateInstance.h"
#include "Styling/AppStyle.h"
#include "EngineUtils.h"
#include "Camera/CameraComponent.h"


#define LOCTEXT_NAMESPACE "FCameraCutSection"


/* FCameraCutSection structors
 *****************************************************************************/

FCameraCutSection::FCameraCutSection(TSharedPtr<ISequencer> InSequencer, TSharedPtr<FTrackEditorThumbnailPool> InThumbnailPool, UMovieSceneSection& InSection) : FViewportThumbnailSection(InSequencer, InThumbnailPool, InSection)
{
	AdditionalDrawEffect = ESlateDrawEffect::NoGamma;
}

FCameraCutSection::~FCameraCutSection()
{
}


/* ISequencerSection interface
 *****************************************************************************/

void FCameraCutSection::SetSingleTime(double GlobalTime)
{
	UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
	if (CameraCutSection && CameraCutSection->HasStartFrame())
	{
		double ReferenceOffsetSeconds = CameraCutSection->GetInclusiveStartFrame() / CameraCutSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
		CameraCutSection->SetThumbnailReferenceOffset(GlobalTime - ReferenceOffsetSeconds);
	}
}

void FCameraCutSection::Tick(const FGeometry& AllottedGeometry, const FGeometry& ClippedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
	if (CameraCutSection)
	{
		if (GetDefault<UMovieSceneUserThumbnailSettings>()->bDrawSingleThumbnails && CameraCutSection->HasStartFrame())
		{
			double ReferenceOffsetSeconds = CameraCutSection->GetInclusiveStartFrame() / CameraCutSection->GetTypedOuter<UMovieScene>()->GetTickResolution() + CameraCutSection->GetThumbnailReferenceOffset();
			ThumbnailCache.SetSingleReferenceFrame(ReferenceOffsetSeconds);
		}
		else
		{
			ThumbnailCache.SetSingleReferenceFrame(TOptional<double>());
		}
	}

	FViewportThumbnailSection::Tick(AllottedGeometry, ClippedGeometry, InCurrentTime, InDeltaTime);
}

void FCameraCutSection::BuildSectionContextMenu(FMenuBuilder& MenuBuilder, const FGuid& ObjectBinding)
{
	FViewportThumbnailSection::BuildSectionContextMenu(MenuBuilder, ObjectBinding);

	UWorld* World = GEditor->GetEditorWorldContext().World();

	if (World == nullptr || !Section->HasStartFrame())
	{
		return;
	}

	AActor* CameraActor = GetCameraForFrame(Section->GetInclusiveStartFrame());

	if (CameraActor)
	{
		MenuBuilder.AddMenuSeparator();

		MenuBuilder.AddMenuEntry(
			FText::Format(LOCTEXT("SelectCameraTextFormat", "Select {0}"), FText::FromString(CameraActor->GetActorLabel())),
			FText::Format(LOCTEXT("SelectCameraTooltipFormat", "Select {0}"), FText::FromString(CameraActor->GetActorLabel())),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this, &FCameraCutSection::HandleSelectCameraMenuEntryExecute, CameraActor))
		);
	}

	// get list of available cameras
	TArray<AActor*> AllCameras;

	for (FActorIterator ActorIt(World); ActorIt; ++ActorIt)
	{
		AActor* Actor = *ActorIt;

		if ((Actor != CameraActor) && Actor->IsListedInSceneOutliner())
		{
			UCameraComponent* CameraComponent = MovieSceneHelpers::CameraComponentFromActor(Actor);
			if (CameraComponent)
			{
				AllCameras.Add(Actor);
			}
		}
	}

	if (AllCameras.Num() == 0)
	{
		return;
	}

	MenuBuilder.BeginSection(NAME_None, LOCTEXT("ChangeCameraMenuText", "Change Camera"));
	{
		for (auto EachCamera : AllCameras)
		{
			FText ActorLabel = FText::FromString(EachCamera->GetActorLabel());

			MenuBuilder.AddMenuEntry(
				FText::Format(LOCTEXT("SetCameraMenuEntryTextFormat", "{0}"), ActorLabel),
				FText::Format(LOCTEXT("SetCameraMenuEntryTooltipFormat", "Assign {0} to this camera cut"), FText::FromString(EachCamera->GetPathName())),
				FSlateIcon(),
				FUIAction(FExecuteAction::CreateRaw(this, &FCameraCutSection::HandleSetCameraMenuEntryExecute, EachCamera))
			);
		}
	}
	MenuBuilder.EndSection();
}


/* FThumbnailSection interface
 *****************************************************************************/

AActor* FCameraCutSection::GetCameraForFrame(FFrameNumber Time) const
{
	UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
	TSharedPtr<ISequencer> Sequencer = SequencerPtr.Pin();

	if (CameraCutSection && Sequencer.IsValid())
	{
		UCameraComponent* CameraComponent = CameraCutSection->GetFirstCamera(*Sequencer, Sequencer->GetFocusedTemplateID());
		if (CameraComponent)
		{
			return CameraComponent->GetOwner();
		}

		FMovieSceneSpawnable* Spawnable = Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene()->FindSpawnable(CameraCutSection->GetCameraBindingID().GetGuid());
		if (Spawnable)
		{
			return Cast<AActor>(Spawnable->GetObjectTemplate());
		}
	}

	return nullptr;
}

FText FCameraCutSection::GetSectionTitle() const
{
	return HandleThumbnailTextBlockText();
}

float FCameraCutSection::GetSectionHeight() const
{
	return FViewportThumbnailSection::GetSectionHeight() + 10.f;
}

FMargin FCameraCutSection::GetContentPadding() const
{
	return FMargin(6.f, 10.f);
}

int32 FCameraCutSection::OnPaintSection(FSequencerSectionPainter& InPainter) const
{
	static const FSlateBrush* FilmBorder = FAppStyle::GetBrush("Sequencer.Section.FilmBorder");

	InPainter.LayerId = InPainter.PaintSectionBackground();
	return FViewportThumbnailSection::OnPaintSection(InPainter);
}

FText FCameraCutSection::HandleThumbnailTextBlockText() const
{
	const AActor* CameraActor = Section->HasStartFrame() ? GetCameraForFrame(Section->GetInclusiveStartFrame()) : nullptr;
	if (CameraActor)
	{
		return FText::FromString(CameraActor->GetActorLabel());
	}

	return FText::GetEmpty();
}


/* FCameraCutSection callbacks
 *****************************************************************************/

void FCameraCutSection::HandleSelectCameraMenuEntryExecute(AActor* InCamera)
{
	GEditor->SelectActor(InCamera, true, true);
}

void FCameraCutSection::HandleSetCameraMenuEntryExecute(AActor* InCamera)
{
	auto Sequencer = SequencerPtr.Pin();

	if (Sequencer.IsValid())
	{
		FGuid ObjectGuid = Sequencer->GetHandleToObject(InCamera, true);

		UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);

		CameraCutSection->SetFlags(RF_Transactional);

		const FScopedTransaction Transaction(LOCTEXT("SetCameraCut", "Set Camera Cut"));

		CameraCutSection->Modify();
	
		CameraCutSection->SetCameraGuid(ObjectGuid);
	
		Sequencer->NotifyMovieSceneDataChanged( EMovieSceneDataChangeType::TrackValueChanged );
	}
}

UCameraComponent* FCameraCutSection::GetViewCamera()
{
	UMovieSceneCameraCutSection* CameraCutSection = Cast<UMovieSceneCameraCutSection>(Section);
	TSharedPtr<ISequencer>       Sequencer        = SequencerPtr.Pin();

	if (CameraCutSection && Sequencer.IsValid())
	{
		return CameraCutSection->GetFirstCamera(*Sequencer, Sequencer->GetFocusedTemplateID());
	}

	return nullptr;
}


#undef LOCTEXT_NAMESPACE
