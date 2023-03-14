// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TakeRecorderSource.h"
#include "TakeRecorderSources.h"
#include "UObject/SoftObjectPtr.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "TakeRecorderWorldSource.generated.h"

class UTexture;
class UTakeRecorderActorSource;

/** A recording source that records world state */
UCLASS(Abstract, config = EditorSettings, DisplayName = "World Recorder")
class UTakeRecorderWorldSourceSettings : public UTakeRecorderSource
{
public:
	GENERATED_BODY()

	UTakeRecorderWorldSourceSettings(const FObjectInitializer& ObjInit);

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	/** Record world settings */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Source")
	bool bRecordWorldSettings;

	/** Add a binding and track for all actors that aren't explicitly being recorded */
	UPROPERTY(config, EditAnywhere, BlueprintReadWrite, Category = "Source")
	bool bAutotrackActors;
};


/** A recording source that records world state */
UCLASS(Category="Actors", meta = (TakeRecorderDisplayName = "World"))
class UTakeRecorderWorldSource : public UTakeRecorderWorldSourceSettings
{
public:
	GENERATED_BODY()

		UTakeRecorderWorldSource(const FObjectInitializer& ObjInit);

private:

	// UTakeRecorderSource
	virtual TArray<UTakeRecorderSource*> PreRecording(ULevelSequence* InSequence, FMovieSceneSequenceID InSequenceID, ULevelSequence* InMasterSequence, FManifestSerializer* InManifestSerializer) override;
	virtual TArray<UTakeRecorderSource*> PostRecording(class ULevelSequence* InSequence, class ULevelSequence* InMasterSequence, const bool bCancelled) override;
	virtual bool SupportsTakeNumber() const override { return false; }
	virtual FText GetDisplayTextImpl() const override;
	virtual bool CanAddSource(UTakeRecorderSources* InSources) const override;

	// This source does not support subscenes (ie. "World Settings subscene"), but the world settings actor would be placed in subscenes if the option is enabled
	virtual bool SupportsSubscenes() const override { return false; }

private:

	/*
	 * Autotrack actors in the world that aren't already being recorded
	 */
	void AutotrackActors(class ULevelSequence* InSequence, UWorld* InWorld);

	TWeakObjectPtr<UTakeRecorderActorSource> WorldSource;
};
