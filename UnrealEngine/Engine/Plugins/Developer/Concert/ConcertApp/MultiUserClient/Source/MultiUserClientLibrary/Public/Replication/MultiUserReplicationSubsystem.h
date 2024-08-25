// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Subsystems/EngineSubsystem.h"
#include "ConcertPropertyChainWrapper.h"
#include "MultiUserReplicationSubsystem.generated.h"

struct FMultiUserObjectReplicationSettings;

namespace UE::MultiUserClientLibrary { class FUObjectAdapterReplicationDiscoverer; }

/** Exposes ways to interact with the Multi-user replication system via Blueprints. */
UCLASS()
class MULTIUSERCLIENTLIBRARY_API UMultiUserReplicationSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()
public:
	
	// This would be the right place to expose additional MU specific replication functions in the future
	
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnServerStateChanged, const FGuid&, EndpointId);
	
	/**
	 * @return Whether the client is replicating the object.
	 * @note An object can be registered but not replicated.
	 * @see UMultiUserSubsystem::GetLocalClientId and UMultiUserSubsystem::GetRemoteClientIds
	 */
	UFUNCTION(BlueprintPure, Category = "Multi-user")
	bool IsReplicatingObject(const FGuid& ClientId, const FSoftObjectPath& ObjectPath) const;

	/**
	 * @return Whether OutFrequency was modified.
	 * @note An object can be registered but not replicated.
	 * @see UMultiUserSubsystem::GetLocalClientId and UMultiUserSubsystem::GetRemoteClientIds
	 */
	UFUNCTION(BlueprintPure, Category = "Multi-user")
	bool GetObjectReplicationFrequency(const FGuid& ClientId, const FSoftObjectPath& ObjectPath, FMultiUserObjectReplicationSettings& OutFrequency);

	/**
	 * @return The properties the client has registered for replication for the object.
	 * @note An object can be registered but not replicated. Use IsReplicatingObject to find out whether the client is replicating the returned properties.
	 * @see UMultiUserSubsystem::GetLocalClientId and UMultiUserSubsystem::GetRemoteClientIds.
	 */
	UFUNCTION(BlueprintPure, Category = "Multi-user")
	TArray<FConcertPropertyChainWrapper> GetPropertiesRegisteredToObject(const FGuid& ClientId, const FSoftObjectPath& ObjectPath) const;

	/**
	 * Gets the objects the client has registered with the server.
	 *
	 * Just because an object is replicated, it does not mean that the object is being replicated.
	 * Objects must be registered with the server so registration is just the first step.
	 * GetReplicatedObjects() will always contain GetRegisteredObjects().
	 * 
	 * @param ClientId The client of which to get the registered objects. 
	 * @return The objects the client has
	 * 
	 * @see UMultiUserSubsystem::GetLocalClientId and UMultiUserSubsystem::GetRemoteClientIds
	 */
	UFUNCTION(BlueprintPure, Category = "Multi-user")
	TArray<FSoftObjectPath> GetRegisteredObjects(const FGuid& ClientId) const;

	/**
	 * Gets the objects that are currently being replicated by the client.
	 *
	 * @note There is a difference between registered and replicated objects! Objects are registered with the server first and later the client
	 * can attempt to start replicating them. GetReplicatedObjects() will always contain GetRegisteredObjects().
	 * 
	 * @param ClientId The client of which to get the replicated objects. 
	 * @return The objects being replicated by the client
	 *
	 * @see UMultiUserSubsystem::GetLocalClientId and UMultiUserSubsystem::GetRemoteClientIds
	 */
	UFUNCTION(BlueprintPure, Category = "Multi-user")
	TArray<FSoftObjectPath> GetReplicatedObjects(const FGuid& ClientId) const;
	
	//~ Begin USubsystem Interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem Interface

private:
	
	/**
	 * Event triggered when the following changes about a client:
	 * - The registered object to properties bindings
	 * - The registered replication frequency setting of an object
	 */
	UPROPERTY(BlueprintAssignable, Category = "Multi-user")
	FOnServerStateChanged OnClientStreamServerStateChanged;
	
	/** Event triggered a client changes the objects it is replicating. */
	UPROPERTY(BlueprintAssignable, Category = "Multi-user")
	FOnServerStateChanged OnClientAuthorityServerStateChanged;

	/**
	 * This is used only for when the adds an object through the Add button in the UI.
	 * 
	 * This allows UObjects, the target being Blueprints, to implement the IConcertReplicationRegistration interface through which MU will use to
	 * auto-add properties when registering an object to a client's replication stream.
	 *
	 * Registered when this subsystem is initialized.
	 */
	TSharedPtr<UE::MultiUserClientLibrary::FUObjectAdapterReplicationDiscoverer> UObjectAdapter;

	void OnClientStreamsChanged(const FGuid& EndpointId) const { OnClientStreamServerStateChanged.Broadcast(EndpointId); }
	void OnClientAuthorityChanged(const FGuid& EndpointId) const { OnClientAuthorityServerStateChanged.Broadcast(EndpointId); }
};
