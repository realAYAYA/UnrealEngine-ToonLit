// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Delegates/Delegate.h"

struct FAssetData;

class AStaticMeshActor;

DECLARE_DELEGATE_ThreeParams(FOnAddProgressiveStageDataCallback, FAssetData AssetData, FString AssetId, AStaticMeshActor* SpawnedActor);

class MEGASCANSPLUGIN_API FBridgeDragDropImpl : public TSharedFromThis<FBridgeDragDropImpl>
{
public:
    FOnAddProgressiveStageDataCallback OnAddProgressiveStageDataDelegate;

    void SetOnAddProgressiveStageData(FOnAddProgressiveStageDataCallback InDelegate);
};

class MEGASCANSPLUGIN_API FBridgeDragDrop
{
public:
    static void Initialize();
    static TSharedPtr<FBridgeDragDropImpl> Instance;
};

#if UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_5_2
#include "AssetRegistry/AssetData.h"
#include "CoreMinimal.h"
#endif
