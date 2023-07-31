// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class AActor;
class IDatasmithActorElement;
class IDatasmithBaseMaterialElement;
class IDatasmithLevelVariantSetsElement;
struct FAssetData;
struct FVREDCppVariant;

using FActorMap = TMap<FName, TArray<TSharedPtr<IDatasmithActorElement>>>;
using FMaterialMap = TMap<FName, TSharedPtr<IDatasmithBaseMaterialElement>>;

class FVREDVariantConverter
{
public:
	static TSharedPtr<IDatasmithLevelVariantSetsElement> ConvertVariants(TArray<FVREDCppVariant>& Vars, FActorMap& ActorsByOriginalName, FMaterialMap& MaterialsByName);
};

