// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Map.h"
#include "Containers/SparseArray.h"
#include "Components/SceneComponent.h"
#include "Net/Serialization/FastArraySerializer.h"
#include "DestructibleHLODComponent.generated.h"


class UMaterialInterface;
class UMaterialInstanceDynamic;
class UTexture2DDynamic;
class FTexture2DDynamicResource;


UCLASS(Abstract, HideCategories=(Tags, Sockets, ComponentTick, ComponentReplication, Activation, Cooking, Events, AssetUserData, Collision), MinimalAPI)
class UWorldPartitionDestructibleHLODComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	ENGINE_API virtual void DestroyActor(int32 ActorIndex) PURE_VIRTUAL(UWorldPartitionDestructibleHLODComponent::DestroyActor, );
	ENGINE_API virtual void DamageActor(int32 ActorIndex, float DamagePercent) PURE_VIRTUAL(UWorldPartitionDestructibleHLODComponent::DamageActor, );
	ENGINE_API virtual void OnDestructionStateUpdated() PURE_VIRTUAL(UWorldPartitionDestructibleHLODComponent::OnDestructionStateUpdated, );

	ENGINE_API const TArray<FName>& GetDestructibleActors() const;
	ENGINE_API void SetDestructibleActors(const TArray<FName>& InDestructibleActors);

protected:
	// Name of the destructible actors from the source cell.
	UPROPERTY()
	TArray<FName> DestructibleActors;
};


// Entry for a damaged actor
USTRUCT()
struct FWorldPartitionDestructibleHLODDamagedActorState : public FFastArraySerializerItem
{
	GENERATED_USTRUCT_BODY()

public:
	static const uint8 MAX_HEALTH = 0xFF;

	FWorldPartitionDestructibleHLODDamagedActorState()
		: ActorIndex(INDEX_NONE)
		, ActorHealth(MAX_HEALTH)
	{
	}

	FWorldPartitionDestructibleHLODDamagedActorState(int32 InActorIndex)
		: ActorIndex(InActorIndex)
		, ActorHealth(MAX_HEALTH)
	{
	}

	bool operator == (const FWorldPartitionDestructibleHLODDamagedActorState& Other) const
	{
		return ActorIndex == Other.ActorIndex && ActorHealth == Other.ActorHealth;
	}

	UPROPERTY()
	int32 ActorIndex;

	UPROPERTY()
	uint8 ActorHealth;
};


// Replicated state of the destructible HLOD
USTRUCT()
struct FWorldPartitionDestructibleHLODState : public FFastArraySerializer
{
	GENERATED_BODY()

public:
	bool NetDeltaSerialize(FNetDeltaSerializeInfo& DeltaParms)
	{
		return FFastArraySerializer::FastArrayDeltaSerialize<FWorldPartitionDestructibleHLODDamagedActorState, FWorldPartitionDestructibleHLODState>(DamagedActors, DeltaParms, *this);
	}

	void Initialize(UWorldPartitionDestructibleHLODComponent* InDestructibleHLODComponent);
	void SetActorHealth(int32 InActorIndex, uint8 InActorHealth);

	const bool IsClient() const { return bIsClient; }
	const bool IsServer() const { return bIsServer; }
	const TArray<uint8>& GetVisibilityBuffer() const { return VisibilityBuffer;	}

	// ~ FFastArraySerializer Contract Begin
	void PostReplicatedAdd(const TArrayView<int32>& AddedIndices, int32 FinalSize);
	void PostReplicatedChange(const TArrayView<int32>& ChangedIndices, int32 FinalSize);
	// ~ FFastArraySerializer Contract End

private:
	void ApplyDamagedActorState(int32 DamagedActorIndex);

private:
	UPROPERTY()
	TArray<FWorldPartitionDestructibleHLODDamagedActorState> DamagedActors;

	UPROPERTY(NotReplicated)
	TObjectPtr<UWorldPartitionDestructibleHLODComponent> OwnerComponent;

	// Server only, map of actors indices to their damage info in the DamagedActors array
	TArray<int32> ActorsToDamagedActorsMapping;	

	// Client only, visibility buffer that is meant to be sent to the GPU
	TArray<uint8> VisibilityBuffer;

	bool bIsServer = false;
	bool bIsClient = false;
	int32 NumDestructibleActors = 0;
};


template<> struct TStructOpsTypeTraits<FWorldPartitionDestructibleHLODState> : public TStructOpsTypeTraitsBase2<FWorldPartitionDestructibleHLODState>
{
	enum { WithNetDeltaSerializer = true };
};


UCLASS(MinimalAPI)
class UWorldPartitionDestructibleHLODMeshComponent : public UWorldPartitionDestructibleHLODComponent
{
	GENERATED_UCLASS_BODY()

public:
	ENGINE_API void SetDestructibleHLODMaterial(UMaterialInterface* InDestructibleMaterial);

	ENGINE_API virtual void DestroyActor(int32 ActorIndex) override;
	ENGINE_API virtual void DamageActor(int32 ActorIndex, float DamagePercent) override;
	ENGINE_API virtual void OnDestructionStateUpdated() override;

private:
	ENGINE_API virtual void BeginPlay() override;

	void SetupVisibilityTexture();
	void UpdateVisibilityTexture();
	static void UpdateVisibilityTexture_RenderThread(FTexture2DDynamicResource* TextureResource, const TArray<uint8>& VisibilityBuffer);

private:
	UPROPERTY()
	TObjectPtr<UMaterialInterface> DestructibleHLODMaterial;

	UPROPERTY(Transient, Replicated)
	FWorldPartitionDestructibleHLODState DestructibleHLODState;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> VisibilityMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2DDynamic> VisibilityTexture;
};
