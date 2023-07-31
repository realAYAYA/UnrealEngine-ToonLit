// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "LiveLinkRole.h"
#include "LiveLinkControllerBase.h"
#include "Templates/SubclassOf.h"

#include "LiveLinkComponentSettings.generated.h"


/**
 * Settings for LiveLink.
 */
UCLASS(config=Game, defaultconfig)
class LIVELINKCOMPONENTS_API ULiveLinkComponentSettings : public UObject
{
	GENERATED_BODY()

public:

	/** Default Controller class to use for the specified role */
	UPROPERTY(config, EditAnywhere, Category = "LiveLink", meta = (AllowAbstract = "false"))
	TMap<TSubclassOf<ULiveLinkRole>, TSubclassOf<ULiveLinkControllerBase>> DefaultControllerForRole;
};
