// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/ColorPropertyTrackEditor.h"
#include "Editor/UnrealEdEngine.h"
#include "UnrealEdGlobals.h"
#include "Sections/ColorPropertySection.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"

FName FColorPropertyTrackEditor::RedName( "R" );
FName FColorPropertyTrackEditor::GreenName( "G" );
FName FColorPropertyTrackEditor::BlueName( "B" );
FName FColorPropertyTrackEditor::AlphaName( "A" );
FName FColorPropertyTrackEditor::SpecifiedColorName( "SpecifiedColor" );

TSharedRef<ISequencerTrackEditor> FColorPropertyTrackEditor::CreateTrackEditor(TSharedRef<ISequencer> InSequencer)
{
	return MakeShareable(new FColorPropertyTrackEditor(InSequencer));
}


TSharedRef<ISequencerSection> FColorPropertyTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FColorPropertyTrackEditor"));
	return MakeShared<FColorPropertySection>(SectionObject, ObjectBinding, GetSequencer());
}


void FColorPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys )
{
	FProperty* Property = PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get();
	if (!Property)
	{
		return;
	}

	const FStructProperty* StructProp = CastField<const FStructProperty>( Property );
	if (!StructProp)
	{
		return;
	}

	FName StructName = StructProp->Struct->GetFName();
	FName PropertyName = Property->GetFName();

	bool bIsFColor = StructName == NAME_Color;

	FLinearColor ColorValue;

	if (bIsFColor)
	{
		ColorValue = FLinearColor( PropertyChangedParams.GetPropertyValue<FColor>() );
	}
	else
	{
		ColorValue = PropertyChangedParams.GetPropertyValue<FLinearColor>();
	}

	if( StructProp->HasMetaData("HideAlphaChannel") )
	{
		ColorValue.A = 1;
	}

	FPropertyPath StructPath = PropertyChangedParams.StructPathToKey;
	FName ChannelName = StructPath.GetNumProperties() != 0 ? StructPath.GetLeafMostProperty().Property->GetFName() : NAME_None;

	const bool bKeyRed   =  ChannelName == NAME_None || ChannelName == RedName   || ChannelName == SpecifiedColorName;
	const bool bKeyGreen =  ChannelName == NAME_None || ChannelName == GreenName || ChannelName == SpecifiedColorName;
	const bool bKeyBlue  =  ChannelName == NAME_None || ChannelName == BlueName  || ChannelName == SpecifiedColorName;
	const bool bKeyAlpha =  ChannelName == NAME_None || ChannelName == AlphaName || ChannelName == SpecifiedColorName;

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(0, ColorValue.R, bKeyRed));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(1, ColorValue.G, bKeyGreen));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(2, ColorValue.B, bKeyBlue));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(3, ColorValue.A, bKeyAlpha));
}

bool FColorPropertyTrackEditor::ModifyGeneratedKeysByCurrentAndWeight(UObject *Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber KeyTime, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const
{
	using namespace UE::MovieScene;

	UMovieSceneColorTrack* ColorTrack = Cast<UMovieSceneColorTrack>(Track);

	if (ColorTrack)
	{
		FSystemInterrogator Interrogator;

		TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, &Interrogator.GetLinker()->EntityManager);

		const FInterrogationChannel InterrogationChannel = Interrogator.AllocateChannel(Object, ColorTrack->GetPropertyBinding());
		Interrogator.ImportTrack(ColorTrack, InterrogationChannel);
		Interrogator.AddInterrogation(KeyTime);

		Interrogator.Update();

		const FMovieSceneTracksComponentTypes* ComponentTypes = FMovieSceneTracksComponentTypes::Get();
		TArray<FIntermediateColor> InterrogatedValues;
		Interrogator.QueryPropertyValues(ComponentTypes->Color, InterrogationChannel, InterrogatedValues);

		FLinearColor Val = InterrogatedValues[0].GetLinearColor();
		FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
		GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.R, Weight);
		GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.G, Weight);
		GeneratedTotalKeys[2]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.B, Weight);
		GeneratedTotalKeys[3]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.A, Weight);

		return true;
	}

	return false;
}
