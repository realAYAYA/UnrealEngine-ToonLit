// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "MSAssetImportData.h"

class FJsonObject;



class IAssetImportFactory
{
public :
	static TSharedPtr<IAssetImportFactory> CreateImporter(EAssetImportType ImporterType) ;
	virtual void ImportAsset(TSharedPtr<FJsonObject> AssetImportJson) = 0;

};

















