// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"

class AStaticMeshActor;

DECLARE_DELEGATE_FourParams(FOnAddProgressiveStageDataCallbackInternal, FAssetData AssetData, FString AssetId, FString AssetType, AStaticMeshActor* SpawnedActor);

class MEGASCANSPLUGIN_API FBridgeDragDropHelperImpl : public TSharedFromThis<FBridgeDragDropHelperImpl>
{
public:
    FOnAddProgressiveStageDataCallbackInternal OnAddProgressiveStageDataDelegate;
	TMap<FString, AActor*> SurfaceToActorMap;
    
    void SetOnAddProgressiveStageData(FOnAddProgressiveStageDataCallbackInternal InDelegate);
};

class MEGASCANSPLUGIN_API FBridgeDragDropHelper
{
public:
	static void Initialize();
	static TSharedPtr<FBridgeDragDropHelperImpl> Instance;
};
