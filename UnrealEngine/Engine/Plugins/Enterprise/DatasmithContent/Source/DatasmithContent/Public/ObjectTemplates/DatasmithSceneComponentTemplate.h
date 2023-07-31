// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "DatasmithSceneComponentTemplate.generated.h"

UCLASS()
class DATASMITHCONTENT_API UDatasmithSceneComponentTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:

	UPROPERTY()
	FTransform RelativeTransform;

	UPROPERTY()
	TEnumAsByte< EComponentMobility::Type > Mobility;

	UPROPERTY()
	TSoftObjectPtr< USceneComponent > AttachParent;

	UPROPERTY()
	bool bVisible;

	UPROPERTY()
	bool bCastShadow = true;

	UPROPERTY()
	TSet<FName> Tags;

	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};