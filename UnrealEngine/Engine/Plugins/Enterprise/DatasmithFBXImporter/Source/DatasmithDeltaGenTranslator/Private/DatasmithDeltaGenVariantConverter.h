// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#define SCENECAMERA_NAME TEXT("Camera")

class AActor;
class IDatasmithActorElement;
class IDatasmithBaseMaterialElement;
class IDatasmithLevelVariantSetsElement;
struct FAssetData;
struct FDeltaGenPosDataState;
struct FDeltaGenVarDataVariantSwitch;

using FActorMap = TMap<FName, TArray<TSharedPtr<IDatasmithActorElement>>>;
using FMaterialMap = TMap<FName, TSharedPtr<IDatasmithBaseMaterialElement>>;

class FDeltaGenVariantConverter
{
public:
	static TSharedPtr<IDatasmithLevelVariantSetsElement> ConvertVariants(TArray<FDeltaGenVarDataVariantSwitch>& Vars, TArray<FDeltaGenPosDataState>& PosStates, FActorMap& ActorsByOriginalName, FMaterialMap& MaterialsByName);
};

