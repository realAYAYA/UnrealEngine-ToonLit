// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"

class AActor;
struct FAssetData;

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

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#endif
