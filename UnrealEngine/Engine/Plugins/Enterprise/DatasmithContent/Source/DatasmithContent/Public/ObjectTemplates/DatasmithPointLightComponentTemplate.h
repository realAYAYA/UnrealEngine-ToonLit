// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/Scene.h"
#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "DatasmithPointLightComponentTemplate.generated.h"

UCLASS()
class DATASMITHCONTENT_API UDatasmithPointLightComponentTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	UDatasmithPointLightComponentTemplate();

	UPROPERTY()
	ELightUnits IntensityUnits;

	UPROPERTY()
	float SourceRadius;

	UPROPERTY()
	float SourceLength;

	UPROPERTY()
	float AttenuationRadius;

	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};
