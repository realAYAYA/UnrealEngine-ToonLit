// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "DatasmithImportOptions.h"
#include "Misc/Guid.h"
#include "Serialization/BulkData.h"
#include "Interfaces/Interface_AssetUserData.h"

#include "DatasmithScene.generated.h"

class ULevelSequence;
class ULevelVariantSets;
class UMaterialFunction;
class UMaterialInterface;
class UStaticMesh;
class UTexture;
class UWorld;

UCLASS()
class DATASMITHCONTENT_API UDatasmithScene : public UObject, public IInterface_AssetUserData
{
	GENERATED_BODY()

public:
	UDatasmithScene();

	virtual ~UDatasmithScene();

#if WITH_EDITORONLY_DATA
	/** Importing data and options used for this Datasmith scene */
	UPROPERTY(EditAnywhere, Instanced, Category=ImportSettings)
	TObjectPtr<class UDatasmithSceneImportData> AssetImportData;

	UPROPERTY()
	int32 BulkDataVersion; // Need an external version number because loading of the bulk data is handled externally

	FByteBulkData DatasmithSceneBulkData;

	/** Map of all the static meshes related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< UStaticMesh > > StaticMeshes;

	/** Map of all the cloth related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< UObject > > Clothes; // UChaosClothAsset

	/** Map of all the textures related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< UTexture > > Textures;

	/** Map of all the material functions related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< UMaterialFunction > > MaterialFunctions;

	/** Map of all the materials related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< UMaterialInterface > > Materials;

	/** Map of all the level sequences related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< ULevelSequence > > LevelSequences;

	/** Map of all the level variant sets related to this Datasmith Scene */
	UPROPERTY(VisibleAnywhere, Category="Datasmith", AdvancedDisplay)
	TMap< FName, TSoftObjectPtr< ULevelVariantSets > > LevelVariantSets;

	/** Array of user data stored with the asset */
	UPROPERTY()
	TArray< TObjectPtr<UAssetUserData> > AssetUserData;
#endif // #if WITH_EDITORONLY_DATA

	//~ Begin UObject Interface
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	//~ End UObject Interface

	/** Register the DatasmithScene to the PreWorldRename callback as needed*/
	void RegisterPreWorldRenameCallback();

	//~ Begin IInterface_AssetUserData Interface
	virtual void AddAssetUserData( UAssetUserData* InUserData ) override;
	virtual void RemoveUserDataOfClass( TSubclassOf<UAssetUserData> InUserDataClass ) override;
	virtual UAssetUserData* GetAssetUserDataOfClass( TSubclassOf<UAssetUserData> InUserDataClass ) override;
	virtual const TArray<UAssetUserData*>* GetAssetUserDataArray() const override;
	//~ End IInterface_AssetUserData Interface

#if WITH_EDITOR
private:
	/** Called before a world is renamed */
	void OnPreWorldRename(UWorld* World, const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags, bool& bShouldFailRename);

	bool bPreWorldRenameCallbackRegistered;
#endif

public:
	virtual void Serialize( FArchive& Archive ) override;
};
