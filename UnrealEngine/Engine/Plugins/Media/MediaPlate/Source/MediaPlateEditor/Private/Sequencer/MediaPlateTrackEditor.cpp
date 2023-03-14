// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MediaPlateTrackEditor.h"

#include "ActorTreeItem.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ISequencer.h"
#include "MediaTexture.h"
#include "MediaPlate.h"
#include "MediaPlateComponent.h"
#include "MediaPlayer.h"
#include "MediaPlaylist.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "MovieSceneMediaSection.h"
#include "MovieSceneMediaTrack.h"
#include "SceneOutlinerModule.h"
#include "Sequencer/MediaTrackEditor.h"

#define LOCTEXT_NAMESPACE "FMediaPlateTrackEditor"

/* FMediaTrackEditor static functions
 *****************************************************************************/

TArray<FAnimatedPropertyKey, TInlineAllocator<1>> FMediaPlateTrackEditor::GetAnimatedPropertyTypes()
{
	return TArray<FAnimatedPropertyKey, TInlineAllocator<1>>({ FAnimatedPropertyKey::FromObjectType(UMediaTexture::StaticClass()) });
}

/* FMediaTrackEditor structors
 *****************************************************************************/

FMediaPlateTrackEditor::FMediaPlateTrackEditor(TSharedRef<ISequencer> InSequencer)
	: FMovieSceneTrackEditor(InSequencer)
	, bGetDurationDelay(false)
{
	OnActorAddedToSequencerHandle = InSequencer->OnActorAddedToSequencer().AddRaw(this, &FMediaPlateTrackEditor::HandleActorAdded);
	FMediaTrackEditor::OnBuildOutlinerEditWidget.AddRaw(this,
		&FMediaPlateTrackEditor::OnBuildOutlinerEditWidget);
}

FMediaPlateTrackEditor::~FMediaPlateTrackEditor()
{
	FMediaTrackEditor::OnBuildOutlinerEditWidget.RemoveAll(this);
}

void FMediaPlateTrackEditor::BuildObjectBindingContextMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	if ((ObjectClass != nullptr) && (ObjectClass->IsChildOf(AMediaPlate::StaticClass())))
	{
		MenuBuilder.BeginSection("Media Plate", LOCTEXT("MediaPlate", "Media Plate"));

		MenuBuilder.AddMenuEntry(
			LOCTEXT("Import", "Import"),
			LOCTEXT("ImportTooltip", "Import tracks from the Media Plate object."),
			FSlateIcon(),
			FUIAction(FExecuteAction::CreateRaw(this,
				&FMediaPlateTrackEditor::ImportObjectBinding, ObjectBindings)));
	
		MenuBuilder.EndSection();
	}
}

void FMediaPlateTrackEditor::BuildObjectBindingTrackMenu(FMenuBuilder& MenuBuilder, const TArray<FGuid>& ObjectBindings, const UClass* ObjectClass)
{
	// Is this a media plate?
	if (ObjectClass != nullptr)
	{
		if (ObjectClass->IsChildOf(AMediaPlate::StaticClass()))
		{
			MenuBuilder.AddMenuEntry(
				LOCTEXT("AddTrack", "Media"),
				LOCTEXT("AddAttachedTooltip", "Adds a media track attached to the object."),
				FSlateIcon(),
				FUIAction(
					FExecuteAction::CreateRaw(this, &FMediaPlateTrackEditor::HandleAddMediaTrackToObjectBindingMenuEntryExecute, ObjectBindings)));
		}
	}
}

bool FMediaPlateTrackEditor::SupportsType(TSubclassOf<UMovieSceneTrack> TrackClass) const
{
	return false;
}

void FMediaPlateTrackEditor::Tick(float DeltaTime)
{
	// Do we have any new sections that need durations?
	if (NewSections.Num() > 0)
	{
		// Can we get the duration?
		if (bGetDurationDelay)
		{
			bGetDurationDelay = false;
		}
		else
		{
			// Loop over all new sections.
			for (int32 Index = 0; Index < NewSections.Num();)
			{
				if (NewSections[Index].Key.IsValid())
				{
					// Try and get the duration.
					if (GetDuration(NewSections[Index].Key, NewSections[Index].Value))
					{
						NewSections.RemoveAtSwap(Index);
					}
					else
					{
						++Index;
					}
				}
				else
				{
					NewSections.RemoveAtSwap(Index);
				}
			}
		}
	}
}

void FMediaPlateTrackEditor::HandleAddMediaTrackToObjectBindingMenuEntryExecute(TArray<FGuid> InObjectBindingIDs)
{
	UMovieScene* FocusedMovieScene = GetFocusedMovieScene();

	if (FocusedMovieScene == nullptr)
	{
		return;
	}

	if (FocusedMovieScene->IsReadOnly())
	{
		return;
	}

	const FScopedTransaction Transaction(NSLOCTEXT("Sequencer", "AddMediaTrack_Transaction", "Add Media Track"));
	FocusedMovieScene->Modify();

	// Loop through all objects.
	for (FGuid InObjectBindingID : InObjectBindingIDs)
	{
		if (InObjectBindingID.IsValid())
		{
			// Add media track.
			UMovieSceneMediaTrack* NewObjectTrack = FocusedMovieScene->AddTrack<UMovieSceneMediaTrack>(InObjectBindingID);
			NewObjectTrack->SetDisplayName(LOCTEXT("MediaTrackName", "Media"));

			if (GetSequencer().IsValid())
			{
				GetSequencer()->OnAddTrack(NewObjectTrack, InObjectBindingID);
			}
		}
	}
}

void FMediaPlateTrackEditor::HandleActorAdded(AActor* Actor, FGuid TargetObjectGuid)
{
	if (Actor)
	{
		if (UMediaPlateComponent* MediaPlateComponent = Actor->FindComponentByClass<UMediaPlateComponent>())
		{
			AddTrackForComponent(MediaPlateComponent);
		}
	}
}

void FMediaPlateTrackEditor::AddTrackForComponent(UMediaPlateComponent* Component)
{
	// Get object.
	UObject* Object = Component->GetOwner();
	FFindOrCreateHandleResult HandleResult = FindOrCreateHandleToObject(Object);
	FGuid ObjectHandle = HandleResult.Handle;

	// Add media track.
	FFindOrCreateTrackResult TrackResult = FindOrCreateTrackForObject(ObjectHandle, UMovieSceneMediaTrack::StaticClass());
	UMovieSceneTrack* Track = TrackResult.Track;
	if (Track != nullptr)
	{
		UMovieSceneMediaTrack* MediaTrack = Cast<UMovieSceneMediaTrack>(Track);
		MediaTrack->SetDisplayName(LOCTEXT("MediaTrackName", "Media"));

		// Populate track.
		UMediaPlaylist* Playlist = Component->MediaPlaylist;
		if (Playlist != nullptr)
		{
			for (int32 Index = 0; Index < Playlist->Num(); ++Index)
			{
				UMediaSource* MediaSource = Playlist->Get(Index);
				if (MediaSource != nullptr)
				{
					UMovieSceneSection* Section = MediaTrack->AddNewMediaSource(*MediaSource, FFrameNumber(0));
					if (Section != nullptr)
					{
						// Start process to get the duration.
						StartGetDuration(MediaSource, Section);

						// Tell the section it has a player proxy.
						UMovieSceneMediaSection* MediaSection = Cast<UMovieSceneMediaSection>(Section);
						if (MediaSection != nullptr)
						{
							MediaSection->bHasMediaPlayerProxy = true;
						}
					}
				}
			}
		}
	}

	// Does the media plate have autoplay?
	if (Component->bAutoPlay)
	{
		// Ask the user if it can be disabled.
		EAppReturnType::Type YesNoCancelReply = FMessageDialog::Open(EAppMsgType::YesNo,
			LOCTEXT("Prompt_DisableAutoplay", "Would you like to disable autoplay on the MediaPlate actor (recommended)?"));

		switch (YesNoCancelReply)
		{
			case EAppReturnType::Yes:
				Component->Modify();
				Component->bAutoPlay = false;
				break;
		}
	}
}

void FMediaPlateTrackEditor::OnRelease()
{
	if (GetSequencer().IsValid())
	{
		if (OnActorAddedToSequencerHandle.IsValid())
		{
			GetSequencer()->OnActorAddedToSequencer().Remove(OnActorAddedToSequencerHandle);
		}
	}

	FMovieSceneTrackEditor::OnRelease();
}

void FMediaPlateTrackEditor::ImportObjectBinding(const TArray<FGuid> ObjectBindings)
{
	// Get actor.
	const TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
	for (const FGuid ObjectBinding : ObjectBindings)
	{
		UObject* BoundObject = SequencerPtr.IsValid() ? SequencerPtr->FindSpawnedObjectOrTemplate(ObjectBinding) : nullptr;
		AActor* Actor = Cast<AActor>(BoundObject);
		if (Actor != nullptr)
		{
			// Get media plate componnent.
			if (UMediaPlateComponent* MediaPlateComponent = Actor->FindComponentByClass<UMediaPlateComponent>())
			{
				// Add tracks for this.
				AddTrackForComponent(MediaPlateComponent);
			}
		}
	}
}

void FMediaPlateTrackEditor::StartGetDuration(UMediaSource* MediaSource, UMovieSceneSection* Section)
{
	// Create media player.
	TStrongObjectPtr<UMediaPlayer> MediaPlayer = TStrongObjectPtr<UMediaPlayer>(
		NewObject<UMediaPlayer>(GetTransientPackage(),
			MakeUniqueObjectName(GetTransientPackage(),
				UMediaPlayer::StaticClass())));

	// Open the media.
	MediaPlayer->PlayOnOpen = false;
	if (MediaPlayer->OpenSource(MediaSource))
	{
		NewSections.Emplace(MediaPlayer, Section);
	}

	// Some players like Electra report that they are closed at this point, so wait a frame.
	bGetDurationDelay = true;
}

bool FMediaPlateTrackEditor::GetDuration(
	TStrongObjectPtr<UMediaPlayer>& MediaPlayer, TWeakObjectPtr<UMovieSceneSection>& NewSection)
{
	bool bIsDone = false;
	
	// Check everything is ok.
	if ((MediaPlayer.IsValid() == false) || (MediaPlayer->HasError()) || (MediaPlayer->IsClosed()) ||
		(NewSection.IsValid() == false))
	{
		bIsDone = true;
	}
	else
	{
		// Get the duration.
		FTimespan Duration = MediaPlayer->GetDuration();
		if (Duration != 0)
		{
			// Once it is non zero, then set the length of the section.
			FFrameRate TickResolution = NewSection->GetTypedOuter<UMovieScene>()->GetTickResolution();
			FFrameNumber StartFrame = NewSection->GetInclusiveStartFrame();
			FFrameNumber EndFrame = StartFrame + (Duration.GetTotalSeconds() * TickResolution).FrameNumber;
			NewSection->SetEndFrame(TRangeBound<FFrameNumber>::Exclusive(EndFrame));
			bIsDone = true;
		}
	}

	return bIsDone;
}

void FMediaPlateTrackEditor::OnBuildOutlinerEditWidget(FMenuBuilder& MenuBuilder)
{
	// Add media plate sub menu.
	MenuBuilder.AddSubMenu(
		LOCTEXT("MediaPlate", "Media Plate"),
		LOCTEXT("MediaPlateTooltip", "Add media from a media plate."),
		FNewMenuDelegate::CreateLambda([this](FMenuBuilder& MenuBuilder)
		{
			// Actor picker options.
			FSceneOutlinerInitializationOptions InitOptions;
			InitOptions.bShowHeaderRow = false;
			InitOptions.bShowSearchBox = true;
			InitOptions.bShowCreateNewFolder = false;
			InitOptions.bFocusSearchBoxWhenOpened = true;
			InitOptions.ColumnMap.Add(FSceneOutlinerBuiltInColumnTypes::Label(), FSceneOutlinerColumnInfo(ESceneOutlinerColumnVisibility::Visible, 0));
			InitOptions.Filters->AddFilterPredicate<FActorTreeItem>(
				FActorTreeItem::FFilterPredicate::CreateLambda([](const AActor* Actor)
				{
					return Actor && Actor->IsA<AMediaPlate>();
				}
				));

			// Create actor picker.
			FSceneOutlinerModule& SceneOutlinerModule = 
				FModuleManager::LoadModuleChecked<FSceneOutlinerModule>("SceneOutliner");
			TSharedRef<SBox> Picker = SNew(SBox)
				.WidthOverride(300.0f)
				.HeightOverride(300.f)
				[
					SceneOutlinerModule.CreateActorPicker(InitOptions,
						FOnActorPicked::CreateLambda([this](AActor* Actor)
						{
							AddMediaPlateToSequencer(Actor);
						}))
				];

			MenuBuilder.AddWidget(Picker, FText::GetEmpty(), true);
		}));
}

void FMediaPlateTrackEditor::AddMediaPlateToSequencer(AActor* Actor)
{
	if (Actor != nullptr)
	{
		TSharedPtr<ISequencer> SequencerPtr = GetSequencer();
		if (SequencerPtr.IsValid())
		{
			TArray<TWeakObjectPtr<AActor>> ActorArray;
			ActorArray.Add(Actor);
			SequencerPtr->AddActors(ActorArray);
		}
	}
}

#undef LOCTEXT_NAMESPACE
