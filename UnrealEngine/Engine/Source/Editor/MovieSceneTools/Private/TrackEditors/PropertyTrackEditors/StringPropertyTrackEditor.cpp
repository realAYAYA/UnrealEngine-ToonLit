// Copyright Epic Games, Inc. All Rights Reserved.

#include "TrackEditors/PropertyTrackEditors/StringPropertyTrackEditor.h"

#include "Channels/MovieSceneStringChannel.h"
#include "Containers/UnrealString.h"
#include "HAL/Platform.h"
#include "KeyPropertyParams.h"
#include "PropertyPath.h"
#include "Templates/RemoveReference.h"
#include "UObject/Field.h"
#include "UObject/UnrealType.h"
#include "UObject/WeakFieldPtr.h"

class ISequencer;
class ISequencerTrackEditor;
class UMovieSceneSection;


TSharedRef<ISequencerTrackEditor> FStringPropertyTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> OwningSequencer )
{
	return MakeShareable(new FStringPropertyTrackEditor(OwningSequencer));
}


void FStringPropertyTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, UMovieSceneSection* SectionToKey, FGeneratedTrackKeys& OutGeneratedKeys )
{
	void* CurrentObject = PropertyChangedParams.ObjectsThatChanged[0];
	void* PropertyValue = nullptr;
	for (int32 i = 0; i < PropertyChangedParams.PropertyPath.GetNumProperties(); i++)
	{
		if (FProperty* Property = PropertyChangedParams.PropertyPath.GetPropertyInfo(i).Property.Get())
		{
			CurrentObject = Property->ContainerPtrToValuePtr<FString>(CurrentObject, 0);
		}
	}

	const FStrProperty* StrProperty = CastField<const FStrProperty>( PropertyChangedParams.PropertyPath.GetLeafMostProperty().Property.Get() );
	if ( StrProperty )
	{
		FString StrPropertyValue = StrProperty->GetPropertyValue(CurrentObject);
		OutGeneratedKeys.Add(FMovieSceneChannelValueSetter::Create<FMovieSceneStringChannel>(0, MoveTemp(StrPropertyValue), true));
	}
}
