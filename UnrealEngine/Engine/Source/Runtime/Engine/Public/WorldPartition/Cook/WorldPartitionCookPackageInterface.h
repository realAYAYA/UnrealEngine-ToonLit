// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreFwd.h"
#include "UObject/Interface.h"
#include "WorldPartitionCookPackageInterface.generated.h"

class UExternalDataLayerAsset;

UINTERFACE(MinimalAPI)
class UWorldPartitionCookPackageObject : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IWorldPartitionCookPackageObject
{
	GENERATED_IINTERFACE_BODY()

public:
#if WITH_EDITOR
	virtual bool IsLevelPackage() const = 0;
	virtual const UExternalDataLayerAsset* GetExternalDataLayerAsset() const = 0;
	virtual FString GetPackageNameToCreate() const = 0;
	virtual bool OnPrepareGeneratorPackageForCook(TArray<UPackage*>& OutModifiedPackages) = 0;
	virtual bool OnPopulateGeneratorPackageForCook(UPackage* InPackage) = 0;
	virtual bool OnPopulateGeneratedPackageForCook(UPackage* InPackage, TArray<UPackage*>& OutModifiedPackages) = 0;
#endif
};