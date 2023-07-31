// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/VectorPropertyTrackEditor.h"
#include "Tracks/MovieSceneVectorTrack.h"
#include "UnrealEdGlobals.h"
#include "Editor/UnrealEdEngine.h"
#include "MovieSceneToolHelpers.h"
#include "Evaluation/MovieScenePropertyTemplate.h"
#include "EntitySystem/Interrogation/MovieSceneInterrogationLinker.h"


FName FFloatVectorPropertyTrackEditor::XName( "X" );
FName FFloatVectorPropertyTrackEditor::YName( "Y" );
FName FFloatVectorPropertyTrackEditor::ZName( "Z" );
FName FFloatVectorPropertyTrackEditor::WName( "W" );

TSharedRef<ISequencerTrackEditor> FFloatVectorPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FFloatVectorPropertyTrackEditor( InSequencer ) );
}

void FFloatVectorPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys )
{
	const FStructProperty* StructProp = CastField<const FStructProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	if (!StructProp)
	{
		return;
	}
	FName StructName = StructProp->Struct->GetFName();

	bool bIsVector2f = StructName == NAME_Vector2f;
	bool bIsVector = (StructName == NAME_Vector3f);
	bool bIsVector4 = (StructName == NAME_Vector4f);

	FVector4f VectorValues;
	int32 Channels;

	if ( bIsVector2f )
	{
		FVector2f Vector2fValue = PropertyChangedParams.GetPropertyValue<FVector2f>();
		VectorValues.X = Vector2fValue.X;
		VectorValues.Y = Vector2fValue.Y;
		Channels = 2;
	}
	else if ( bIsVector )
	{
		FVector3f Vector3DValue = PropertyChangedParams.GetPropertyValue<FVector3f>();
		VectorValues.X = Vector3DValue.X;
		VectorValues.Y = Vector3DValue.Y;
		VectorValues.Z = Vector3DValue.Z;
		Channels = 3;
	}
	else // if ( bIsVector4 )
	{
		VectorValues = PropertyChangedParams.GetPropertyValue<FVector4f>();
		Channels = 4;
	}

	FPropertyPath StructPath = PropertyChangedParams.StructPathToKey;
	FName ChannelName = StructPath.GetNumProperties() != 0 ? StructPath.GetLeafMostProperty().Property->GetFName() : NAME_None;

	const bool bKeyX = ChannelName == NAME_None || ChannelName == XName;
	const bool bKeyY = ChannelName == NAME_None || ChannelName == YName;

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(0, VectorValues.X, bKeyX));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(1, VectorValues.Y, bKeyY));

	if ( Channels >= 3 )
	{
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(2, VectorValues.Z, ChannelName == NAME_None || ChannelName == ZName));
	}

	if ( Channels >= 4 )
	{
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneFloatChannel>(3, VectorValues.W, ChannelName == NAME_None || ChannelName == WName));
	}
}

void FFloatVectorPropertyTrackEditor::InitializeNewTrack( UMovieSceneFloatVectorTrack* NewTrack, FPropertyChangedParams PropertyChangedParams )
{
	FPropertyTrackEditor::InitializeNewTrack( NewTrack, PropertyChangedParams );
	const FStructProperty* StructProp = CastField<const FStructProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	FName StructName = StructProp->Struct->GetFName();

	if ( StructName == NAME_Vector2f )
	{
		NewTrack->SetNumChannelsUsed( 2 );
	}
	if ( StructName == NAME_Vector3f)		// LWC_TODO: Investigate why only this is the float variant
	{
		NewTrack->SetNumChannelsUsed( 3 );
	}
	if ( StructName == NAME_Vector4 )
	{
		NewTrack->SetNumChannelsUsed( 4 );
	}
}

bool FFloatVectorPropertyTrackEditor::ModifyGeneratedKeysByCurrentAndWeight(UObject *Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber KeyTime, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const
{
	using namespace UE::MovieScene;

	UMovieSceneFloatVectorTrack* VectorTrack = Cast<UMovieSceneFloatVectorTrack>(Track);

	if (VectorTrack)
	{
		FSystemInterrogator Interrogator;

		TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, &Interrogator.GetLinker()->EntityManager);

		const FInterrogationChannel InterrogationChannel = Interrogator.AllocateChannel(Object, VectorTrack->GetPropertyBinding());
		Interrogator.ImportTrack(VectorTrack, InterrogationChannel);
		Interrogator.AddInterrogation(KeyTime);

		Interrogator.Update();

		const FMovieSceneTracksComponentTypes* ComponentTypes = FMovieSceneTracksComponentTypes::Get();
		TArray<FFloatIntermediateVector> InterrogatedValues;
		Interrogator.QueryPropertyValues(ComponentTypes->FloatVector, InterrogationChannel, InterrogatedValues);

		switch (VectorTrack->GetNumChannelsUsed())
		{
		case 2:
			{
				FVector2f Val(InterrogatedValues[0].X, InterrogatedValues[0].Y);
				FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
				GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.X, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Y, Weight);
			}
			break;
		case 3:
			{
				FVector3f Val(InterrogatedValues[0].X, InterrogatedValues[0].Y, InterrogatedValues[0].Z);
				FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
				GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.X, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Y, Weight);
				GeneratedTotalKeys[2]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Z, Weight);
			}
			break;
		case 4:
			{
				FVector4 Val(InterrogatedValues[0].X, InterrogatedValues[0].Y, InterrogatedValues[0].Z, InterrogatedValues[0].W);
				FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
				GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.X, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Y, Weight);
				GeneratedTotalKeys[2]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Z, Weight);
				GeneratedTotalKeys[3]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.W, Weight);

			}
			break;
		}

		return true;
	}

	return false;
}


FName FDoubleVectorPropertyTrackEditor::XName( "X" );
FName FDoubleVectorPropertyTrackEditor::YName( "Y" );
FName FDoubleVectorPropertyTrackEditor::ZName( "Z" );
FName FDoubleVectorPropertyTrackEditor::WName( "W" );


TSharedRef<ISequencerTrackEditor> FDoubleVectorPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FDoubleVectorPropertyTrackEditor( InSequencer ) );
}

void FDoubleVectorPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys )
{
	const FStructProperty* StructProp = CastField<const FStructProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	if (!StructProp)
	{
		return;
	}
	FName StructName = StructProp->Struct->GetFName();

	bool bIsVector2D = StructName == NAME_Vector2D;
	bool bIsVector = (StructName == NAME_Vector3d
			|| StructName == NAME_Vector
			);

	bool bIsVector4 = (StructName == NAME_Vector4d
		|| StructName == NAME_Vector4
		);

	ensure(bIsVector2D || bIsVector || bIsVector4);

	FVector4d VectorValues;
	int32 Channels;
	
	if (bIsVector2D)
	{
		FVector2D Vector2DValue = PropertyChangedParams.GetPropertyValue<FVector2D>();
		VectorValues.X = Vector2DValue.X;
		VectorValues.Y = Vector2DValue.Y;
		Channels = 2;
	}
	else if (bIsVector)
	{
		FVector3d Vector3DValue = PropertyChangedParams.GetPropertyValue<FVector3d>();
		VectorValues.X = Vector3DValue.X;
		VectorValues.Y = Vector3DValue.Y;
		VectorValues.Z = Vector3DValue.Z;
		Channels = 3;
	}
	else // if (bIsVector4)
	{
		VectorValues = PropertyChangedParams.GetPropertyValue<FVector4d>();
		Channels = 4;
	}

	FPropertyPath StructPath = PropertyChangedParams.StructPathToKey;
	FName ChannelName = StructPath.GetNumProperties() != 0 ? StructPath.GetLeafMostProperty().Property->GetFName() : NAME_None;

	const bool bKeyX = ChannelName == NAME_None || ChannelName == XName;
	const bool bKeyY = ChannelName == NAME_None || ChannelName == YName;

	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(0, VectorValues.X, bKeyX));
	OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(1, VectorValues.Y, bKeyY));

	if ( Channels >= 3 )
	{
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(2, VectorValues.Z, ChannelName == NAME_None || ChannelName == ZName));
	}

	if ( Channels >= 4)
	{
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneDoubleChannel>(3, VectorValues.W, ChannelName == NAME_None || ChannelName == WName));
	}
}

void FDoubleVectorPropertyTrackEditor::InitializeNewTrack( UMovieSceneDoubleVectorTrack* NewTrack, FPropertyChangedParams PropertyChangedParams )
{
	FPropertyTrackEditor::InitializeNewTrack( NewTrack, PropertyChangedParams );
	const FStructProperty* StructProp = CastField<const FStructProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	FName StructName = StructProp->Struct->GetFName();

	if (StructName == NAME_Vector2D)
	{
		NewTrack->SetNumChannelsUsed( 2 );
	}
	if (StructName == NAME_Vector3d
			|| StructName == NAME_Vector
			)
	{
		NewTrack->SetNumChannelsUsed( 3 );
	}	
	if ( StructName == NAME_Vector4d
		|| StructName == NAME_Vector4
	)
	{
		NewTrack->SetNumChannelsUsed( 4 );
	}
}

bool FDoubleVectorPropertyTrackEditor::ModifyGeneratedKeysByCurrentAndWeight(UObject *Object, UMovieSceneTrack *Track, UMovieSceneSection* SectionToKey, FFrameNumber KeyTime, FGeneratedTrackKeys& GeneratedTotalKeys, float Weight) const
{
	using namespace UE::MovieScene;

	UMovieSceneDoubleVectorTrack* VectorTrack = Cast<UMovieSceneDoubleVectorTrack>(Track);

	if (VectorTrack)
	{
		FSystemInterrogator Interrogator;

		TGuardValue<FEntityManager*> DebugVizGuard(GEntityManagerForDebuggingVisualizers, &Interrogator.GetLinker()->EntityManager);

		Interrogator.ImportTrack(VectorTrack, FInterrogationChannel::Default());
		Interrogator.AddInterrogation(KeyTime);

		Interrogator.Update();

		const FMovieSceneTracksComponentTypes* ComponentTypes = FMovieSceneTracksComponentTypes::Get();
		TArray<FDoubleIntermediateVector> InterrogatedValues;
		Interrogator.QueryPropertyValues(ComponentTypes->DoubleVector, InterrogatedValues);

		switch (VectorTrack->GetNumChannelsUsed())
		{
		case 2:
			{
				FVector2D Val(InterrogatedValues[0].X, InterrogatedValues[0].Y);
				FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
				GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.X, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Y, Weight);
			}
			break;
		case 3:
			{
				FVector3d Val(InterrogatedValues[0].X, InterrogatedValues[0].Y, InterrogatedValues[0].Z);
				FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
				GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.X, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Y, Weight);
				GeneratedTotalKeys[2]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Z, Weight);
			}
			break;
		case 4:
			{
				FVector4d Val(InterrogatedValues[0].X, InterrogatedValues[0].Y, InterrogatedValues[0].Z, InterrogatedValues[0].W);
				FMovieSceneChannelProxy& Proxy = SectionToKey->GetChannelProxy();
				GeneratedTotalKeys[0]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.X, Weight);
				GeneratedTotalKeys[1]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Y, Weight);
				GeneratedTotalKeys[2]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.Z, Weight);
				GeneratedTotalKeys[3]->ModifyByCurrentAndWeight(Proxy, KeyTime, (void *)&Val.W, Weight);
			}
			break;
		}

		return true;
	}

	return false;
}
