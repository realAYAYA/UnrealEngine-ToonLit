// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/DeveloperSettings.h"

#include "Templates/SubclassOf.h"

#include "LiveLinkSequencerSettings.generated.h"

class ULiveLinkControllerBase;
class UMovieSceneLiveLinkControllerTrackRecorder;


/**
 * Settings for LiveLink Sequence Editor
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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "LiveLinkControllerBase.h"
#include "MovieSceneLiveLinkControllerTrackRecorder.h"
#endif
