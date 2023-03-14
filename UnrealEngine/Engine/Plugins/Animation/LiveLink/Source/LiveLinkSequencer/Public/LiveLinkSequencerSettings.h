// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "LiveLinkControllerBase.h"
#include "MovieSceneLiveLinkControllerTrackRecorder.h"
#include "Templates/SubclassOf.h"

#include "LiveLinkSequencerSettings.generated.h"


/**
 * Settings for LiveLink Sequencer
 */
UCLASS(config=Game)
class LIVELINKSEQUENCER_API ULiveLinkSequencerSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	//~ Begin UDeveloperSettings interface
	virtual FName GetCategoryName() const;
#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FName GetSectionName() const override;
#endif
	//~ End UDeveloperSettings interface

public:

	/** Default Track Recorder class to use for the specified LiveLink controller */
	UPROPERTY(config, EditAnywhere, Category = "LiveLink", meta = (AllowAbstract = "false"))
	TMap<TSubclassOf<ULiveLinkControllerBase>, TSubclassOf<UMovieSceneLiveLinkControllerTrackRecorder>> DefaultTrackRecordersForController;
};
