// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "DatasmithDecalComponentTemplate.generated.h"

UCLASS()
class DATASMITHCONTENT_API UDatasmithDecalComponentTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	UDatasmithDecalComponentTemplate();

	UPROPERTY()
	int32 SortOrder;

	UPROPERTY()
	FVector DecalSize;

	UPROPERTY()
	TObjectPtr<class UMaterialInterface> Material;

	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
};
