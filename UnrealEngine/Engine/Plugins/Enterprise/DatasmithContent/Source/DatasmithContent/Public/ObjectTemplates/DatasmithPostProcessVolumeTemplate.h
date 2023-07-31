// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectTemplates/DatasmithCineCameraComponentTemplate.h"
#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "DatasmithPostProcessVolumeTemplate.generated.h"

/**
 * Represents an APostProcessVolume
 */
UCLASS()
class DATASMITHCONTENT_API UDatasmithPostProcessVolumeTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	UDatasmithPostProcessVolumeTemplate()
		: UDatasmithObjectTemplate(true)
	{}

	UPROPERTY()
	FDatasmithPostProcessSettingsTemplate Settings;

	UPROPERTY()
	uint32 bEnabled:1;

	UPROPERTY()
	uint32 bUnbound:1;

	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};