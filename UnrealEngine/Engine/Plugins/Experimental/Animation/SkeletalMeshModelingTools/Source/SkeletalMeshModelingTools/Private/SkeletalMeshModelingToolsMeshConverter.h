// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "BoneContainer.h"
#include "Factories/Factory.h"
#include "ReferenceSkeleton.h"
#include "Interfaces/Interface_BoneReferenceSkeletonProvider.h"

#include "SkeletalMeshModelingToolsMeshConverter.generated.h"


class USkeleton;
class UStaticMesh;
struct FAssetData;


UCLASS(Hidden, MinimalAPI)
class USkeletonFromStaticMeshFactory :
	public UFactory
{
	GENERATED_BODY()
public:
	USkeletonFromStaticMeshFactory(const FObjectInitializer& InObjectInitializer);

	// UFactory overrides
	virtual UObject* FactoryCreateNew(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags InFlags,
		UObject* InContext,
		FFeedbackContext* InWarn
		) override;

	virtual bool ShouldShowInNewMenu() const override { return false; }
	
	UPROPERTY()
	TObjectPtr<UStaticMesh> StaticMesh;
};


UCLASS(Hidden, MinimalAPI)
class USkeletalMeshFromStaticMeshFactory :
	public UFactory
{
	GENERATED_BODY()
public:
	USkeletalMeshFromStaticMeshFactory(const FObjectInitializer& InObjectInitializer);

	// UFactory overrides
	virtual UObject* FactoryCreateNew(
		UClass* InClass,
		UObject* InParent,
		FName InName,
		EObjectFlags InFlags,
		UObject* InContext,
		FFeedbackContext* InWarn
		) override;
	
	virtual bool ShouldShowInNewMenu() const override { return false; }

	FReferenceSkeleton ReferenceSkeleton;

	UPROPERTY()
	TObjectPtr<USkeleton> Skeleton;
	
	UPROPERTY()
	TObjectPtr<UStaticMesh> StaticMesh;

	UPROPERTY()
	FName BindBoneName;
};


UENUM()
enum class EReferenceSkeletonImportOption
{
	CreateNew,
	UseExistingSkeleton,
	UseExistingSkeletalMesh
};


UCLASS(config=EditorPerProjectUserSettings, HideCategories=Object, MinimalAPI)
class UStaticMeshToSkeletalMeshConvertOptions :
	public UObject,
	public IBoneReferenceSkeletonProvider
{
	GENERATED_BODY()
public:

	UPROPERTY(EditAnywhere, Config, Category="Import")
	EReferenceSkeletonImportOption SkeletonImportOption = EReferenceSkeletonImportOption::CreateNew;

	UPROPERTY(EditAnywhere, Config, Category="Import", Meta=(AllowedClasses="/Script/Engine.Skeleton", EditCondition="SkeletonImportOption==EReferenceSkeletonImportOption::UseExistingSkeleton", EditConditionHides))
	FSoftObjectPath Skeleton;

	UPROPERTY(EditAnywhere, Config, Category="Import", Meta=(AllowedClasses="/Script/Engine.SkeletalMesh", EditCondition="SkeletonImportOption==EReferenceSkeletonImportOption::UseExistingSkeletalMesh", EditConditionHides))
	FSoftObjectPath SkeletalMesh;

	UPROPERTY(EditAnywhere, Config, Category="Import", meta=(EditCondition="SkeletonImportOption!=EReferenceSkeletonImportOption::CreateNew", EditConditionHides))
	FBoneReference BindingBoneName;

	// IBoneReferenceSkeletonProvider implementation.
	virtual USkeleton* GetSkeleton(bool& bInvalidSkeletonIsError, const IPropertyHandle* PropertyHandle) override;
	
	// UObject overrides.
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	FSimpleDelegate SkeletonProviderChanged;
};


/** Converts a static mesh to a skeletal mesh, creating both a skeletal mesh and a skeleton assets that link up. */
void ConvertStaticMeshAssetsToSkeletalMeshesInteractive(const TArray<FAssetData>& InStaticMeshAssets);
