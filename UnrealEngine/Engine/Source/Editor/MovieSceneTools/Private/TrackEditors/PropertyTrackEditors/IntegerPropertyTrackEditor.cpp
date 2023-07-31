// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/IntegerPropertyTrackEditor.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"
#include "Tracks/MovieSceneIntegerTrack.h"


TSharedRef<ISequencerTrackEditor> FIntegerPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FIntegerPropertyTrackEditor(OwningSequencer));
}


void FIntegerPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys )
{
	const int32 KeyedValue = PropertyChangedParams.GetPropertyValue<int32>();
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneIntegerChannel>(0, KeyedValue, true));
}

bool FIntegerPropertyTrackEditor::ModifyGeneratedKeysByCurrentAndWeight(UObject *Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber KeyTime, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const
{
	using namespace UE::MovieScene;

	UMovieSceneIntegerTrack* IntegerTrack = Cast<UMovieSceneIntegerTrack>(Track);

	if (IntegerTrack)
	{
		FSystemInterrogator Interrogator;

		TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, &Interrogator.GetLinker()->EntityManager);

		const FInterrogationChannel InterrogationChannel = Interrogator.AllocateChannel(Object, IntegerTrack->GetPropertyBinding());
		Interrogator.ImportTrack(Track, InterrogationChannel);
		Interrogator.AddInterrogation(KeyTime);

		Interrogator.Update();

		const FMovieSceneTracksComponentTypes* ComponentTypes = FMovieSceneTracksComponentTypes::Get();
		TArray<int32> InterrogatedValues;
		Interrogator.QueryPropertyValues(ComponentTypes->Integer, InterrogationChannel, InterrogatedValues);

		int32 CurValue = InterrogatedValues[0];
		FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
		GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&CurValue, Weight);

		return true;
	}

	return false;
}
