// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Animation/SkinWeightProfile.h"
#include "ClothingAsset.h"
#include "CoreMinimal.h"
#include "InterchangeFactoryBase.h"
#include "Rendering/SkeletalMeshLODImporterData.h"
#include "UObject/Object.h"
#include "UObject/ObjectMacros.h"

#include "InterchangeSkeletalMeshFactory.generated.h"

class UInterchangeSkeletalMeshFactoryNode;
class USkeletalMesh;
class USkeleton;

class INTERCHANGEIMPORT_API FInterchangeSkeletalMeshPostImportTask : public FInterchangePostImportTask
{
public:
	virtual void Execute() override;

	TObjectPtr<USkeletalMesh> SkeletalMesh;
	bool bReImportAlternateSkinWeights = false;
};

UCLASS(BlueprintType)
class INTERCHANGEIMPORT_API UInterchangeSkeletalMeshFactory : public UInterchangeFactoryBase
{
	GENERATED_BODY()
public:
	struct FImportAssetObjectLODData
	{
		int32 LodIndex = INDEX_NONE;
		TArray<FName> ExistingOriginalPerSectionMaterialImportName;
#if WITH_EDITOR
		TArray<SkeletalMeshImportData::FMaterial> ImportedMaterials;
		TArray<SkeletalMeshImportData::FBone> RefBonesBinary;
#endif
		bool bUseTimeZeroAsBindPose = false;
		bool bDiffPose = false;
	};

	struct FImportAssetObjectData
	{
		bool bIsReImport = false;
		USkeleton* SkeletonReference = nullptr;
		bool bApplyGeometryOnly = false;
		TArray<FImportAssetObjectLODData> LodDatas;

		TArray<FSkinWeightProfileInfo> ExistingSkinWeightProfileInfos;
		TArray<ClothingAssetUtils::FClothingAssetMeshBinding> ExistingClothingBindings;
#if WITH_EDITOR
		TArray<FSkeletalMeshImportData> ExistingAlternateImportDataPerLOD;
#endif

		bool IsValid() const;
	};
	//////////////////////////////////////////////////////////////////////////
	// Interchange factory base interface begin

	virtual UClass* GetFactoryClass() const override;
	virtual EInterchangeFactoryAssetType GetFactoryAssetType() override { return EInterchangeFactoryAssetType::Meshes; }
	virtual FImportAssetResult BeginImportAsset_GameThread(const FImportAssetObjectParams& Arguments) override;
	virtual FImportAssetResult ImportAsset_Async(const FImportAssetObjectParams& Arguments) override;
	virtual FImportAssetResult EndImportAsset_GameThread(const FImportAssetObjectParams& Arguments) override;
	virtual void Cancel() override;
	virtual void SetupObject_GameThread(const FSetupObjectParams& Arguments) override;
	virtual void FinalizeObject_GameThread(const FSetupObjectParams& Arguments) override;
	virtual bool GetSourceFilenames(const UObject* Object, TArray<FString>& OutSourceFilenames) const override;
	virtual bool SetSourceFilename(const UObject* Object, const FString& SourceFilename, int32 SourceIndex) const override;
	virtual bool SetReimportSourceIndex(const UObject* Object, int32 SourceIndex) const override;

	// Interchange factory base interface end
	//////////////////////////////////////////////////////////////////////////
private:
	FEvent* SkeletalMeshLockPropertiesEvent = nullptr;

	FImportAssetObjectData ImportAssetObjectData;
};


