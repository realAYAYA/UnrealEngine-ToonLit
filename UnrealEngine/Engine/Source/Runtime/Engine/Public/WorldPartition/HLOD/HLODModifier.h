// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Templates/SubclassOf.h"

#include "HLODModifier.generated.h"


class UActorComponent;
class UHLODBuilder;
struct FHLODBuildContext;

/**
 * Base class for all runtime HLOD modifiers
 */
UCLASS(Abstract, MinimalAPI)
class UWorldPartitionHLODModifier : public UObject
{
	 GENERATED_UCLASS_BODY()

public:
	ENGINE_API virtual bool CanModifyHLOD(TSubclassOf<UHLODBuilder> InHLODBuilderClass) const PURE_VIRTUAL(UWorldPartitionHLODModifier::CanModifyHLOD, return false;);
	
	ENGINE_API virtual void BeginHLODBuild(const FHLODBuildContext& InHLODBuildContext) PURE_VIRTUAL(UWorldPartitionHLODModifier::BeginHLODBuild, );
	ENGINE_API virtual void EndHLODBuild(TArray<UActorComponent*>& InOutComponents) PURE_VIRTUAL(UWorldPartitionHLODModifier::EndHLODBuild, );
};
