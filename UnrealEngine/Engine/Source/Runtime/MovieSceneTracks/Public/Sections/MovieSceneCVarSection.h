// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "Containers/UnrealString.h"
#include "CoreMinimal.h"
#include "EntitySystem/IMovieSceneEntityProvider.h"
#include "EntitySystem/MovieSceneEntityIDs.h"
#include "MovieSceneSection.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"

#include "MovieSceneCVarSection.generated.h"

class UMovieSceneEntitySystemLinker;
class UObject;
struct FFrame;

USTRUCT()
struct FMovieSceneCVarOverrides
{
	GENERATED_BODY()

	MOVIESCENETRACKS_API void SetFromString(const FString& InString);

	MOVIESCENETRACKS_API FString GetString() const;

	/** The name of the console variable and the value, separated by ' ' or '=', ie: "foo.bar=1" or "foo.bar 1". */
	UPROPERTY(EditAnywhere, Category="Console Variables")
	TMap<FString, FString> ValuesByCVar;
};

/**
 * A CVar section is responsible for applying a user-supplied value to the specified cvar, and then restoring the previous value after the section ends.
 */
UCLASS(MinimalAPI)
class UMovieSceneCVarSection 
	: public UMovieSceneSection
	, public IMovieSceneEntityProvider
{
	GENERATED_BODY()

public:
	/** Constructs a new console variable section */
	UMovieSceneCVarSection(const FObjectInitializer& Init);

	UFUNCTION(BlueprintCallable, Category="CVars")
	MOVIESCENETRACKS_API void SetFromString(const FString& InString);

	UFUNCTION(BlueprintCallable, Category="CVars")
	MOVIESCENETRACKS_API FString GetString() const;

private:

	virtual void ImportEntityImpl(UMovieSceneEntitySystemLinker* EntityLinker, const FEntityImportParams& Params, FImportedEntity* OutImportedEntity) override;

public:

	/** The name of the console variable and the value, separated by ' ' or '=', ie: "foo.bar=1" or "foo.bar 1". */
	UPROPERTY(EditAnywhere,  meta=(MultiLine=true), Category="Console Variables")
	FMovieSceneCVarOverrides ConsoleVariables;

	friend class UMovieSceneCVarTrackInstance;
};
