// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "CineCameraActor.h"

#include "DatasmithCineCameraActorTemplate.generated.h"

USTRUCT()
struct DATASMITHCONTENT_API FDatasmithCameraLookatTrackingSettingsTemplate
{
	GENERATED_BODY()

public:
	UPROPERTY()
	uint8 bEnableLookAtTracking : 1;

	UPROPERTY()
	uint8 bAllowRoll : 1;

	UPROPERTY()
	TSoftObjectPtr< AActor > ActorToTrack;

public:
	FDatasmithCameraLookatTrackingSettingsTemplate();

	void Apply( FCameraLookatTrackingSettings* Destination, FDatasmithCameraLookatTrackingSettingsTemplate* PreviousTemplate );
	void Load( const FCameraLookatTrackingSettings& Source );
	bool Equals( const FDatasmithCameraLookatTrackingSettingsTemplate& Other ) const;
};

UCLASS()
class DATASMITHCONTENT_API UDatasmithCineCameraActorTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	UDatasmithCineCameraActorTemplate()
		: UDatasmithObjectTemplate(true)
	{}

	UPROPERTY()
	FDatasmithCameraLookatTrackingSettingsTemplate LookatTrackingSettings;

	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};