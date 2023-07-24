// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/Interface.h"
#include "HLODDestruction.generated.h"


class UWorldPartitionDestructibleHLODComponent;


USTRUCT(BlueprintType)
struct ENGINE_API FWorldPartitionHLODDestructionTag
{
	GENERATED_BODY()

	FWorldPartitionHLODDestructionTag()
		: HLODDestructionComponent()
		, ActorIndex(INDEX_NONE)
	{
	}

	bool IsValid() const
	{
		return HLODDestructionComponent != nullptr && ActorIndex != INDEX_NONE;
	}

	UPROPERTY()
	TObjectPtr<UWorldPartitionDestructibleHLODComponent> HLODDestructionComponent;

	UPROPERTY()
	int32 ActorIndex;
};


UINTERFACE(NotBlueprintable, meta=(DisplayName="Destructible in HLOD Interface"))
class ENGINE_API UWorldPartitionDestructibleInHLODInterface : public UInterface
{
	GENERATED_BODY()
};


class ENGINE_API IWorldPartitionDestructibleInHLODInterface
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintNativeEvent, meta=(BlueprintInternalUseOnly = "true"))
	void SetHLODDestructionTag(const FWorldPartitionHLODDestructionTag& InDestructionTag);

	UFUNCTION(BlueprintNativeEvent, meta=(BlueprintInternalUseOnly = "true"))
	FWorldPartitionHLODDestructionTag GetHLODDestructionTag() const;
};


UCLASS()
class ENGINE_API UWorldPartitionDestructibleInHLODSupportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "HLOD Destruction")
	static void DestroyInHLOD(const TScriptInterface<IWorldPartitionDestructibleInHLODInterface>& DestructibleInHLOD);

	UFUNCTION(BlueprintCallable, Category="HLOD Destruction")
	static void DamageInHLOD(const TScriptInterface<IWorldPartitionDestructibleInHLODInterface>& DestructibleInHLOD, float DamagePercent);
};