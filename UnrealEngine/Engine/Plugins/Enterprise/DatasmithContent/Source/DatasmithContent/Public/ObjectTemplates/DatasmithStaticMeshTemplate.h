// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Engine/EngineTypes.h"
#include "DatasmithObjectTemplate.h"

#include "DatasmithStaticMeshTemplate.generated.h"

struct FMeshSectionInfo;
struct FMeshSectionInfoMap;
struct FStaticMaterial;

USTRUCT()
struct DATASMITHCONTENT_API FDatasmithMeshBuildSettingsTemplate
{
	GENERATED_BODY()

public:
	UPROPERTY()
	uint8 bUseMikkTSpace:1;

	UPROPERTY()
	uint8 bRecomputeNormals:1;

	UPROPERTY()
	uint8 bRecomputeTangents:1;

	UPROPERTY()
	uint8 bRemoveDegenerates:1;

	UPROPERTY()
	uint8 bUseHighPrecisionTangentBasis:1;

	UPROPERTY()
	uint8 bUseFullPrecisionUVs:1;

	UPROPERTY()
	uint8 bGenerateLightmapUVs:1;

	UPROPERTY()
	int32 MinLightmapResolution;

	UPROPERTY()
	int32 SrcLightmapIndex;

	UPROPERTY()
	int32 DstLightmapIndex;

public:
	FDatasmithMeshBuildSettingsTemplate();

	void Apply( FMeshBuildSettings* Destination, FDatasmithMeshBuildSettingsTemplate* PreviousTemplate );
	void Load( const FMeshBuildSettings& Source );
	bool Equals( const FDatasmithMeshBuildSettingsTemplate& Other ) const;
};

USTRUCT()
struct DATASMITHCONTENT_API FDatasmithStaticMaterialTemplate
{
	GENERATED_BODY()

public:
	FDatasmithStaticMaterialTemplate();

	UPROPERTY()
	FName MaterialSlotName;

	UPROPERTY()
	TObjectPtr<class UMaterialInterface> MaterialInterface;

	void Apply( FStaticMaterial* Destination, FDatasmithStaticMaterialTemplate* PreviousTemplate );
	void Load( const FStaticMaterial& Source );
	bool Equals( const FDatasmithStaticMaterialTemplate& Other ) const;
};

USTRUCT()
struct DATASMITHCONTENT_API FDatasmithMeshSectionInfoTemplate
{
	GENERATED_BODY()

public:
	FDatasmithMeshSectionInfoTemplate();

	UPROPERTY()
	int32 MaterialIndex;

	void Apply( FMeshSectionInfo* Destination, FDatasmithMeshSectionInfoTemplate* PreviousTemplate );
	void Load( const FMeshSectionInfo& Source );
	bool Equals( const FDatasmithMeshSectionInfoTemplate& Other ) const;
};

USTRUCT()
struct DATASMITHCONTENT_API FDatasmithMeshSectionInfoMapTemplate
{
	GENERATED_BODY()

	UPROPERTY()
	TMap< uint32, FDatasmithMeshSectionInfoTemplate > Map;

	void Apply( FMeshSectionInfoMap* Destination, FDatasmithMeshSectionInfoMapTemplate* PreviousTemplate );
	void Load( const FMeshSectionInfoMap& Source );
	bool Equals( const FDatasmithMeshSectionInfoMapTemplate& Other ) const;
};

UCLASS()
class DATASMITHCONTENT_API UDatasmithStaticMeshTemplate : public UDatasmithObjectTemplate
{
	GENERATED_BODY()

public:
	virtual UObject* UpdateObject( UObject* Destination, bool bForce = false ) override;
	virtual void Load( const UObject* Source ) override;
	virtual bool Equals( const UDatasmithObjectTemplate* Other ) const override;

	UPROPERTY( VisibleAnywhere, Category = StaticMesh )
	FDatasmithMeshSectionInfoMapTemplate SectionInfoMap;

	UPROPERTY( VisibleAnywhere, Category = StaticMesh )
	int32 LightMapCoordinateIndex = INDEX_NONE;

	UPROPERTY( VisibleAnywhere, Category = StaticMesh )
	int32 LightMapResolution = 0;

	UPROPERTY( VisibleAnywhere, Category = StaticMesh )
	TArray< FDatasmithMeshBuildSettingsTemplate > BuildSettings;

	UPROPERTY( VisibleAnywhere, Category = StaticMesh )
	TArray< FDatasmithStaticMaterialTemplate > StaticMaterials;
};
