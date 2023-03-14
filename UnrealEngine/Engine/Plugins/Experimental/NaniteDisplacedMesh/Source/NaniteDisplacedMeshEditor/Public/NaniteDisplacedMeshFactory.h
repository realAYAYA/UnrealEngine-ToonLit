// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Factories/Factory.h"
#include "NaniteDisplacedMeshFactory.generated.h"

class UNaniteDisplacedMesh;
struct FNaniteDisplacedMeshParams;

UCLASS(hidecategories = Object)
class NANITEDISPLACEDMESHEDITOR_API UNaniteDisplacedMeshFactory : public UFactory
{
	GENERATED_BODY()

public:
	UNaniteDisplacedMeshFactory();

	//~ Begin UFactory Interface
	virtual UObject* FactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn) override;
	static UNaniteDisplacedMesh* StaticFactoryCreateNew(UClass* Class, UObject* InParent, FName Name, EObjectFlags Flags, UObject* Context, FFeedbackContext* Warn);

	bool bCreateReadOnlyAsset = false;
};

enum class ELinkDisplacedMeshAssetSetting : uint8
{
	LinkAgainstPersistentAsset,
	CanLinkAgainstPersistentAndTransientAsset,
	LinkAgainstTransientAsset,
	LinkAgainstExistingPersistentAsset,
};

NANITEDISPLACEDMESHEDITOR_API UNaniteDisplacedMesh* LinkDisplacedMeshAsset(
	UNaniteDisplacedMesh* ExistingDisplacedMesh,
	const FNaniteDisplacedMeshParams& InParameters,
	const FString& DisplacedMeshFolder,
	ELinkDisplacedMeshAssetSetting LinkDisplacedMeshAssetSetting = ELinkDisplacedMeshAssetSetting::LinkAgainstPersistentAsset
);

extern NANITEDISPLACEDMESHEDITOR_API const TCHAR* LinkedDisplacedMeshAssetNamePrefix;

NANITEDISPLACEDMESHEDITOR_API FString GenerateLinkedDisplacedMeshAssetName(const FNaniteDisplacedMeshParams& InParameters);

NANITEDISPLACEDMESHEDITOR_API FGuid GetAggregatedId(const FNaniteDisplacedMeshParams& DisplacedMeshParams);
NANITEDISPLACEDMESHEDITOR_API FGuid GetAggregatedId(const UNaniteDisplacedMesh& DisplacedMesh);

NANITEDISPLACEDMESHEDITOR_API FString GetAggregatedIdString(const FNaniteDisplacedMeshParams& DisplacedMeshParams);
NANITEDISPLACEDMESHEDITOR_API FString GetAggregatedIdString(const UNaniteDisplacedMesh& DisplacedMesh);
