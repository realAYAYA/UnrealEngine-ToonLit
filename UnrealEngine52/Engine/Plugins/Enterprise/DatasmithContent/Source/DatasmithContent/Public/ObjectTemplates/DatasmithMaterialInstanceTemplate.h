// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ObjectTemplates/DatasmithObjectTemplate.h"

#include "UObject/ObjectMacros.h"
#include "UObject/SoftObjectPtr.h"

#include "DatasmithMaterialInstanceTemplate.generated.h"

struct FStaticParameterSet;
class UMaterialInterface;
class UMaterialInstanceConstant;
class UTexture;

USTRUCT()
struct DATASMITHCONTENT_API FDatasmithStaticParameterSetTemplate
{
	GENERATED_BODY();

public:
	UPROPERTY()
	TMap< FName, bool > StaticSwitchParameters;

public:
	void Apply( UMaterialInstanceConstant* Destination, FDatasmithStaticParameterSetTemplate* PreviousTemplate );
	void Load( const UMaterialInstanceConstant& Source, bool bOverridesOnly = true );
	void LoadRebase( const UMaterialInstanceConstant& Source, const FDatasmithStaticParameterSetTemplate& ComparedTemplate, const FDatasmithStaticParameterSetTemplate* MergedTemplate);
	bool Equals( const FDatasmithStaticParameterSetTemplate& Other ) const;
};

/**
 * Applies material instance data to a material instance if it hasn't changed since the last time we've applied a template.
 * Supports Scalar parameters, Vector parameters, Texture parameters and Static parameters
 */
UCLASS()
class DATASMITHCONTENT_API UDatasmithMaterialInstanceTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;
	
	UPROPERTY()
	TSoftObjectPtr< UMaterialInterface > ParentMaterial;

	UPROPERTY()
	TMap< FName, float > ScalarParameterValues;

	UPROPERTY()
	TMap< FName, FLinearColor > VectorParameterValues;

	UPROPERTY()
	TMap< FName, TSoftObjectPtr< UTexture > > TextureParameterValues;

	UPROPERTY()
	FDatasmithStaticParameterSetTemplate StaticParameters;

protected:
	virtual void LoadRebase(const UObject* Source, const UDatasmithObjectTemplate* BaseTemplate, bool bMergeTemplate) override;
	virtual bool HasSameBase(const UDatasmithObjectTemplate* Other) const override;
	/**
	 * Loads all the source object properties into the template, regardless if they are different from the default values or not.
	**/
	virtual void LoadAll(const UObject* Source);
};
