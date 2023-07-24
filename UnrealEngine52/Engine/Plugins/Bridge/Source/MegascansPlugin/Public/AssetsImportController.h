// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Templates/SharedPointer.h"

class MEGASCANSPLUGIN_API FAssetsImportController
{
private:
	FAssetsImportController() = default;
	static TSharedPtr<FAssetsImportController> AssetsImportController;
	TArray<FString> SupportedAssetTypes = {
		TEXT("3d"),
		TEXT("3dplant"),
		TEXT("atlas"),
		TEXT("surface")
	};

public:	
	static TSharedPtr<FAssetsImportController> Get();
	void DataReceived(const FString DataFromBridge);
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "CoreMinimal.h"
#include "Engine/StaticMeshActor.h"
#include "MSAssetImportData.h"
#include "Materials/MaterialInstanceConstant.h"
#endif
