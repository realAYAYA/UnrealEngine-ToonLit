// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "Components/SkyLightComponent.h"

#include "DatasmithSkyLightComponentTemplate.generated.h"

UCLASS()
class DATASMITHCONTENT_API UDatasmithSkyLightComponentTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	UDatasmithSkyLightComponentTemplate();

	UPROPERTY()
	TEnumAsByte< ESkyLightSourceType > SourceType;

	UPROPERTY()
	int32 CubemapResolution;

	UPROPERTY()
	TObjectPtr<class UTextureCube> Cubemap;

	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};