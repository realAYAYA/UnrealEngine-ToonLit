// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "UObject/GCObject.h"

class UTexture2D;
class UMaterial;
class UMaterialFunctionInterface;
struct FScopedSlowTask;

class FVTConversionWorker : public FGCObject
{
public:
	FVTConversionWorker(bool bInConvertBackward = false) : bConvertBackward(bInConvertBackward){}

	bool bConvertBackward;

	// Fill this array intially with some textures you want to see converted to vt
	TArray<UTexture2D*> UserTextures;

	// These will be filled by calling FilterList
	TArray<UTexture2D*> Textures;
	TArray<UMaterial *>Materials;
	TArray<UMaterialFunctionInterface *> Functions;
	TArray<UTexture2D*> SizeRejectedTextures;// The original selection of the user filtered by textures that match the threshold size
	TArray<UTexture2D*> MaterialRejectedTextures; // Textures that could not be converted due to their usage in materials (connected to unsupported property)

	struct FAuditTrail
	{
		FAuditTrail(UObject *SetObject, const FString &SetString) : Destination(SetObject), PathDescription(SetString) {}
		UObject *Destination; //Next object in the trail
		FString PathDescription; //Description on how we go there
	};

	TMap<UObject*, FAuditTrail> AuditTrail;

	void SetConversionDirection(bool bInConvertBackward) { bConvertBackward = bInConvertBackward; }

	void FilterList(int32 SizeThreshold);

	// Based on the filtered lists do the actual VT conversion
	void DoConvert();

	void AddReferencedObjects( FReferenceCollector& Collector ) override;
	virtual FString GetReferencerName() const override
	{
		return TEXT("FVTConversionWorker");
	}

private:
	void FindAllTexturesAndMaterials_Iteration(TArray<UMaterial*>& InAffectedMaterials,
		TArray<UMaterialFunctionInterface*>& InAffectedFunctions,
		TArray<UTexture2D*>& InAffectedTextures,
		TArray<UTexture2D*>& InInvalidTextures,
		FScopedSlowTask& Task);

	void FindAllTexturesAndMaterials(TArray<UMaterial *> &OutAffectedMaterials, TArray<UMaterialFunctionInterface *> &OutAffectedFunctions, TArray<UTexture2D *> &OutAffectedTextures);

};

void GetReferencersData(UObject *Object, UClass *MatchClass, TArray<FAssetData> &OutAssetDatas);