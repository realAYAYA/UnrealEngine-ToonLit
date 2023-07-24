// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once
#include "Templates/SharedPointer.h"

class FJsonObject;




class FImportUAssetNormal 
	
{

private :
	FImportUAssetNormal() = default;
	static TSharedPtr<FImportUAssetNormal> ImportUassetNormalInst;

public:
	static TSharedPtr<FImportUAssetNormal> Get();
	void ImportAsset(TSharedPtr<FJsonObject> AssetImportJson);
};
