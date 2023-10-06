// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Engine/EngineTypes.h"
#include "ActorReplicationBridge.generated.h"

#if UE_WITH_IRIS

class UActorComponent;
class UNetDriver;
class UWorld;
class UNetDriver;
class UIrisObjectReferencePackageMap;

namespace UE::Net::Private
{
	struct FActorReplicationBridgeCreationHeader;
	struct FActorCreationHeader;
	struct FSubObjectCreationHeader;
}

namespace UE::Net
{

// If actor should be replicated using IRIS or old replication system
ENGINE_API bool ShouldUseIrisReplication(const UObject* Actor);

}

/** Parameters passed to UActorReplicationBridge::BeginReplication. */
struct FActorBeginReplicationParams
{
	FActorBeginReplicationParams() : bIncludeInLevelGroupFilter(1U) {}

	uint32 bIncludeInLevelGroupFilter : 1U;
};

#endif // UE_WITH_IRIS

UCLASS(Transient, MinimalAPI)
class UActorReplicationBridge final : public UObjectReplicationBridge
{
	GENERATED_BODY()

public:
	ENGINE_API UActorReplicationBridge();
	virtual ENGINE_API ~UActorReplicationBridge() override;

#if UE_WITH_IRIS

	ENGINE_API static UActorReplicationBridge* Create(UNetDriver* NetDriver);

	/** Sets the net driver for the bridge. */
	ENGINE_API virtual void SetNetDriver(UNetDriver* const InNetDriver) override;
	
	/** Get net driver used by the bridge .*/
	inline UNetDriver* GetNetDriver() const { return NetDriver; }

	/** Begin replication of an actor and its registered ActorComponents and SubObjects. */
	ENGINE_API FNetRefHandle BeginReplication(AActor* Instance, const FActorBeginReplicationParams& Params);

	/** Stop replicating an actor. Will destroy handle for actor and registered subobjects. */
	ENGINE_API void EndReplication(AActor* Actor, EEndPlayReason::Type EndPlayReason);
		
	/**
	 * Begin replication of an ActorComponent and its registered SubObjects, 
	 * if the ActorComponent already is replicated any set NetObjectConditions will be updated.
	*/
	ENGINE_API FNetRefHandle BeginReplication(FNetRefHandle OwnerHandle, UActorComponent* ActorComponent);

	/** Stop replicating an ActorComponent and its associated SubObjects. */
	ENGINE_API void EndReplicationForActorComponent(UActorComponent* ActorComponent, EEndReplicationFlags EndReplicationFlags = EEndReplicationFlags::None);
	
	/** Get object reference packagemap. Used in special cases where serialization hasn't been converted to use NetSerializers.  */
	UIrisObjectReferencePackageMap* GetObjectReferencePackageMap() const { return ObjectReferencePackageMap; }
	
	using UObjectReplicationBridge::EndReplication;

protected:

	// UObjectReplicationBridge
	virtual void Initialize(UReplicationSystem* ReplicationSystem) override;
	virtual bool WriteCreationHeader(UE::Net::FNetSerializationContext& Context, FNetRefHandle Handle) override;
	virtual FCreationHeader* ReadCreationHeader(UE::Net::FNetSerializationContext& Context) override;
	virtual FObjectReplicationBridgeInstantiateResult BeginInstantiateFromRemote(FNetRefHandle SubObjectOwnerNetHandle, const UE::Net::FNetObjectResolveContext& ResolveContext, const FCreationHeader* InHeader) override;
	virtual bool OnInstantiatedFromRemote(UObject* Instance, const FCreationHeader* InHeader, uint32 ConnectionId) const override;
	virtual void EndInstantiateFromRemote(FNetRefHandle Handle) override;
	virtual void DestroyInstanceFromRemote(UObject* Instance, EReplicationBridgeDestroyInstanceReason DestroyReason, EReplicationBridgeDestroyInstanceFlags DestroyFlags) override;
	virtual void GetInitialDependencies(FNetRefHandle Handle, FNetDependencyInfoArray& OutDependencies) const override;
	virtual bool RemapPathForPIE(uint32 ConnectionId, FString& Path, bool bReading) const override;
	virtual bool ObjectLevelHasFinishedLoading(UObject* Object) const override;
	virtual bool IsAllowedToDestroyInstance(const UObject* Instance) const override;

	virtual float GetPollFrequencyOfRootObject(const UObject* ReplicatedObject) const override;

private:
	void GetActorCreationHeader(const AActor* Actor, UE::Net::Private::FActorCreationHeader& Header) const;
	void GetSubObjectCreationHeader(const UObject* Object, UE::Net::Private::FSubObjectCreationHeader& Header) const;

	void OnMaxTickRateChanged(UNetDriver* InNetDriver, int32 NewMaxTickRate, int32 OldMaxTickRate);

private:
	UNetDriver* NetDriver;

	UIrisObjectReferencePackageMap* ObjectReferencePackageMap;

	uint32 SpawnInfoFlags;

#endif // UE_WITH_IRIS
};
