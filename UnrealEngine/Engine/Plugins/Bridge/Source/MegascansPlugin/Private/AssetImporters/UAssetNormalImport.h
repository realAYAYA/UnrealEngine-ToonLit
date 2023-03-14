// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MSAssetImportData.h"




class FImportUAssetNormal 
	
{

private :
	FImportUAssetNormal() = default;
	static TSharedPtr<FImportUAssetNormal> ImportUassetNormalInst;

public:
	static TSharedPtr<FImportUAssetNormal> Get();
	void ImportAsset(TSharedPtr<FJsonObject> AssetImportJson);
};