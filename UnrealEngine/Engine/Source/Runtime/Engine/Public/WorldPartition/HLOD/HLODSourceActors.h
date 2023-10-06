// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HLODSourceActors.generated.h"


class ULevelStreaming;
class UHLODLayer;


UCLASS(Abstract, MinimalAPI)
class UWorldPartitionHLODSourceActors : public UObject
{
	GENERATED_UCLASS_BODY()

public:
#if WITH_EDITOR
	ENGINE_API virtual ULevelStreaming* LoadSourceActors(bool& bOutDirty) const PURE_VIRTUAL(UWorldPartitionHLODSourceActors::LoadSourceActors, return nullptr; );
	ENGINE_API virtual uint32 GetHLODHash() const;

	ENGINE_API void SetHLODLayer(const UHLODLayer* HLODLayer);
	ENGINE_API const UHLODLayer* GetHLODLayer() const;
#endif

private:
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TObjectPtr<const UHLODLayer> HLODLayer;
#endif
};
