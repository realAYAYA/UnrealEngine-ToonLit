// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/ActorReferencePropertyTrackEditor.h"

#include "GameFramework/Actor.h"
#include "ISequencer.h"
#include "KeyPropertyParams.h"
#include "Misc/Guid.h"
#include "Misc/Optional.h"
#include "MovieSceneObjectBindingID.h"
#include "MovieSceneSpawnableAnnotation.h"
#include "MovieSceneTrackEditor.h"
#include "Sections/MovieSceneActorReferenceSection.h"
#include "Templates/Casts.h"

class ISequencerTrackEditor;
class UMovieSceneSection;
class UObject;


TSharedRef<ISequencerTrackEditor> FActorReferencePropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FActorReferencePropertyTrackEditor(OwningSequencer));
}

void FActorReferencePropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys )
{
	// Care is taken here to ensure that GetPropertyValue is templated on UObject* which causes it to use the correct instantiation of GetPropertyValueImpl
	AActor* NewReferencedActor = Cast<AActor>(PropertyChangedParams.GetPropertyValue<UObject*>());
	if ( NewReferencedActor != nullptr )
	{
		TSharedPtr<ISequencer> SequencerPtr = GetSequencer();

		FMovieSceneObjectBindingID Binding;

		TOptional<FMovieSceneSpawnableAnnotation> Spawnable = FMovieSceneSpawnableAnnotation::Find(NewReferencedActor);
		if (Spawnable.IsSet())
		{
			// Check whether the spawnable is underneath the current sequence, if so, we can remap it to a local sequence ID
			Binding = UE::MovieScene::FRelativeObjectBindingID(SequencerPtr->GetFocusedTemplateID(), Spawnable->SequenceID, Spawnable->ObjectBindingID, *SequencerPtr);
		}
		else
		{
			FGuid ParentActorId = FindOrCreateHandleToObject(NewReferencedActor).Handle;
			Binding = UE::MovieScene::FRelativeObjectBindingID(ParentActorId);
		}

		if (Binding.IsValid())
		{
			FMovieSceneActorReferenceKey NewKey;
			NewKey.Object = Binding;
			OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneActorReferenceData>(0, NewKey, true));
		}
	}
}
