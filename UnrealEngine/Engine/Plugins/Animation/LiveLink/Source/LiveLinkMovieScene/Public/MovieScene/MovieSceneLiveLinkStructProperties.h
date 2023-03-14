// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Channels/MovieSceneBoolChannel.h"
#include "Channels/MovieSceneByteChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Channels/MovieSceneStringChannel.h"
#include "LiveLinkTypes.h"

#include "MovieSceneLiveLinkStructProperties.generated.h"

class IMovieSceneLiveLinkPropertyHandler;

namespace LiveLinkPropertiesUtils
{
	LIVELINKMOVIESCENE_API TSharedPtr< IMovieSceneLiveLinkPropertyHandler> CreatePropertyHandler(const UScriptStruct& InStruct, FLiveLinkPropertyData* InPropertyData);
}

USTRUCT()
struct FLiveLinkPropertyData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FName PropertyName;

	UPROPERTY()
	TArray<FMovieSceneFloatChannel> FloatChannel;

	UPROPERTY()
	TArray<FMovieSceneStringChannel> StringChannel;

	UPROPERTY()
	TArray<FMovieSceneIntegerChannel> IntegerChannel;

	UPROPERTY()
	TArray<FMovieSceneBoolChannel> BoolChannel;

	UPROPERTY()
	TArray<FMovieSceneByteChannel> ByteChannel;

	int32 GetChannelCount() const
	{
		return FloatChannel.Num() + StringChannel.Num() + IntegerChannel.Num() + BoolChannel.Num() + ByteChannel.Num();
	}
};

USTRUCT()
struct FLiveLinkSubSectionData
{
	GENERATED_BODY()

public:
	UPROPERTY()
	TArray<FLiveLinkPropertyData> Properties;
};

