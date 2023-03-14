// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "MSAssetImportData.h"



class IAssetImportFactory
{
public :
	static TSharedPtr<IAssetImportFactory> CreateImporter(EAssetImportType ImporterType) ;
	virtual void ImportAsset(TSharedPtr<FJsonObject> AssetImportJson) = 0;

};

















