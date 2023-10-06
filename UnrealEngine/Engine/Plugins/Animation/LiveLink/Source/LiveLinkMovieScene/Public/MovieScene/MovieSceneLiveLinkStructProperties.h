// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneLiveLinkStructProperties.generated.h"

struct FLiveLinkPropertyData;
struct FMovieSceneBoolChannel;
struct FMovieSceneByteChannel;
struct FMovieSceneFloatChannel;
struct FMovieSceneIntegerChannel;
struct FMovieSceneStringChannel;

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
	LIVELINKMOVIESCENE_API FLiveLinkPropertyData();
	LIVELINKMOVIESCENE_API ~FLiveLinkPropertyData();

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


#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "Channels/MovieSceneBoolChannel.h"
#include "Channels/MovieSceneByteChannel.h"
#include "Channels/MovieSceneFloatChannel.h"
#include "Channels/MovieSceneIntegerChannel.h"
#include "Channels/MovieSceneStringChannel.h"
#include "CoreMinimal.h"
#include "LiveLinkTypes.h"
#endif
