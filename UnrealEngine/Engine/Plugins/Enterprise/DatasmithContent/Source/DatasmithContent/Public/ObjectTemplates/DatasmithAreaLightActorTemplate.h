// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithAreaLightActor.h"
#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "Engine/Scene.h"

#include "DatasmithAreaLightActorTemplate.generated.h"

UCLASS()
class DATASMITHCONTENT_API UDatasmithAreaLightActorTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	UPROPERTY()
	EDatasmithAreaLightActorType LightType;

	UPROPERTY()
	EDatasmithAreaLightActorShape LightShape;

	UPROPERTY()
	FVector2D Dimensions;

	UPROPERTY()
	FLinearColor Color;

	UPROPERTY()
	float Intensity;

	UPROPERTY()
	ELightUnits IntensityUnits;

	UPROPERTY()
	float Temperature;

	UPROPERTY()
	TSoftObjectPtr< class UTextureLightProfile > IESTexture;

	UPROPERTY()
	bool bUseIESBrightness;

	UPROPERTY()
	float IESBrightnessScale;

	UPROPERTY()
	FRotator Rotation;

	UPROPERTY()
	float SourceRadius;

	UPROPERTY()
	float SourceLength;

	UPROPERTY()
	float AttenuationRadius;

public:
	UDatasmithAreaLightActorTemplate();

	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};