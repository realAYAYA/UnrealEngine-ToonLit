// Copyright Epic Games, Inc. All Rights Reserved.

/**
* Sequencer Animation Track Support interface - this is required for animation track to work
*/

#pragma once
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "HAL/Platform.h"
#include "UObject/Interface.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "INodeAndChannelMappings.generated.h"

class FName;
class UMovieSceneSection;
class UObject;

/** Enumeration specifying the control type */
enum class FFBXControlRigTypeProxyEnum : uint8
{
	Bool,
	Float,
	Vector2D,
	Position,
	Scale,
	Rotator,
	Transform,
	TransformNoScale,
	EulerTransform,
	Integer
};

class UMovieSceneTrack;
struct FMovieSceneBoolChannel;
struct FMovieSceneByteChannel;
struct FMovieSceneDoubleChannel;
struct FMovieSceneFloatChannel;
struct FMovieSceneIntegerChannel;

// For import data onto channels directly
struct FFBXNodeAndChannels
{
	UMovieSceneTrack* MovieSceneTrack;
	FFBXControlRigTypeProxyEnum ControlType;
	FString NodeName;
	TArray< FMovieSceneDoubleChannel*> DoubleChannels;
	TArray< FMovieSceneFloatChannel*> FloatChannels;
	//will really only have one ever.
	TArray< FMovieSceneBoolChannel*> BoolChannels;
	TArray< FMovieSceneIntegerChannel*> IntegerChannels;
	TArray< FMovieSceneByteChannel*> EnumChannels;

};


UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class MOVIESCENE_API UNodeAndChannelMappings : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class MOVIESCENE_API INodeAndChannelMappings
{
	GENERATED_IINTERFACE_BODY()

	/** Get The Node And Mappings for this Track. Note Callee is responsible for deleting 
	*  InSection The section from which to get the nodes and the channels from. If empty, up to implementor to decide which section to use, usually the Section To Key.
	*/
	virtual TArray<FFBXNodeAndChannels>*  GetNodeAndChannelMappings(UMovieSceneSection* InSection)  = 0;
	
	/** Get Selected Nodes */
	virtual void GetSelectedNodes(TArray<FName>& OutSelectedNodes) = 0;
};
