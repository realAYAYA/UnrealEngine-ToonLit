// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

struct FToolMenuSection;
class FMenuBuilder;

class FAssetTypeActions_StaticMesh : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_StaticMesh", "Static Mesh"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 255, 255); }
	virtual UClass* GetSupportedClass() const override { return UStaticMesh::StaticClass(); }
	virtual void GetActions(const TArray<UObject*>& InObjects, FToolMenuSection& Section) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Basic; }
	virtual class UThumbnailInfo* GetThumbnailInfo(UObject* Asset) const override;
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	// End IAssetTypeActions

private:

	/** Handler for when CopyLODDData is selected */
	void ExecuteCopyLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects);
	
	/** Whether there is a valid static mesh to copy LOD from */
	bool CanCopyLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects) const;

	/** Handler for when PasteLODDData is selected */
	void ExecutePasteLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Whether there is a valid static meshes to copy LOD to*/
	bool CanPasteLODSettings(TArray<TWeakObjectPtr<UStaticMesh>> Objects) const;

	/** Handler for when SaveGeneratedLODsInPackage is selected */
	void ExecuteSaveGeneratedLODsInPackage(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Handler for when RemoveVertexColors is selected */
	void ExecuteRemoveVertexColors(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Handler for when NaniteEnable is modified */
	void ModifyNaniteEnable(TArray<TWeakObjectPtr<UStaticMesh>> Objects, bool bNaniteEnable);

	/** Handler for when NaniteEnable is selected */
	void ExecuteNaniteEnable(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Handler for when NaniteDisable is selected */
	void ExecuteNaniteDisable(TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Handler to provide the Nanite sub-menu */
	void GetNaniteMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UStaticMesh>> Meshes);

	/** Handler to provide the list of LODs that can be imported or reimported */
	void GetImportLODMenu(class FMenuBuilder& MenuBuilder,TArray<TWeakObjectPtr<UStaticMesh>> Objects, const bool bWithNewFile);

	/** Handler to provide the LOD sub-menu. Hides away LOD actions - includes Import LOD sub menu */
	void GetLODMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UStaticMesh>> Meshes);

	/** Handler for calling import methods */
	static void ExecuteImportMeshLOD(UObject* Mesh, int32 LOD, bool bReimportWithNewFile);

	/** Builds the High Res mesh sub-menu as part of the LOD menu */
	void GetImportHiResMenu(class FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<UStaticMesh>> Objects);

	/** Handles import and reimport of the high res source model */
	static void ExecuteImportHiResMesh(UStaticMesh* Mesh);

	/** Handles reimport of the high res source model with a new specified file. */
	static void ExecuteReimportHiResMeshWithNewFile(UStaticMesh* StaticMesh);

	/** Handles removing the high res source model from the mesh */
	static void ExecuteRemoveHiResMesh(UStaticMesh* Mesh);

private:

	TWeakObjectPtr<UStaticMesh> LODCopyMesh;	
};
