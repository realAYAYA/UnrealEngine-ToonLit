// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "NetworkPredictionCheck.h"

#include "NetworkPredictionReplicationProxy.generated.h"

struct FNetworkPredictionProxy;
class UPackageMap;

// Target of replication
enum class EReplicationProxyTarget: uint8
{
	ServerRPC,			// Client -> Server
	AutonomousProxy,	// Owning/Controlling client
	SimulatedProxy,		// Non owning client
	Replay,				// Replay net driver
};

inline FString LexToString(EReplicationProxyTarget A)
{
	return *UEnum::GetValueAsString(TEXT("NetworkPrediction.EReplicationProxyTarget"), A);
}

// The parameters for NetSerialize that are passed around the system. Everything should use this, expecting to have to add more.
struct FNetSerializeParams
{
	FNetSerializeParams(FArchive& InAr) : Ar(InAr) { }
	FArchive& Ar;
};

// Redirects NetSerialize to a dynamically set NetSerializeFunc.
// This is how we hook into the replication systems role-based serialization
USTRUCT()
struct NETWORKPREDICTION_API FReplicationProxy
{
	GENERATED_BODY()

	void Init(FNetworkPredictionProxy* InNetSimProxy, EReplicationProxyTarget InReplicationTarget);
	bool NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess);
	void OnPreReplication();	
	bool Identical(const FReplicationProxy* Other, uint32 PortFlags) const;

	TFunction<void(const FNetSerializeParams& P)> NetSerializeFunc;
	FNetworkPredictionProxy* NetSimProxy = nullptr;

private:

	EReplicationProxyTarget ReplicationTarget;
	int32 CachedPendingFrame = INDEX_NONE;
};

template<>
struct TStructOpsTypeTraits<FReplicationProxy> : public TStructOpsTypeTraitsBase2<FReplicationProxy>
{
	enum
	{
		WithNetSerializer = true,
		WithIdentical = true,
	};
};

// Collection of each replication proxy
struct FReplicationProxySet
{
	FReplicationProxy* ServerRPC = nullptr;
	FReplicationProxy* AutonomousProxy = nullptr;
	FReplicationProxy* SimulatedProxy = nullptr;
	FReplicationProxy* Replay = nullptr;

	void UnbindAll() const
	{
		npCheckSlow(ServerRPC && AutonomousProxy && SimulatedProxy && Replay);
		ServerRPC->NetSerializeFunc = nullptr;
		AutonomousProxy->NetSerializeFunc = nullptr;
		SimulatedProxy->NetSerializeFunc = nullptr;
		Replay->NetSerializeFunc = nullptr;
	}
};

// -------------------------------------------------------------------------------------------------------------------------------
//	FServerRPCProxyParameter
//	Used for the client->Server RPC. Since this is instantiated on the stack by the replication system prior to net serializing,
//	we have no opportunity to point the RPC parameter to the member variables we want. So we serialize into a generic temp byte buffer
//	and move them into the real buffers on the component in the RPC body (via ::NetSerialzeToProxy).
// -------------------------------------------------------------------------------------------------------------------------------
USTRUCT()
struct NETWORKPREDICTION_API FServerReplicationRPCParameter
{
	GENERATED_BODY()

	// Receive flow: ctor() -> NetSerializetoProxy
	FServerReplicationRPCParameter() : Proxy(nullptr)	{ }
	void NetSerializeToProxy(FReplicationProxy& InProxy);

	// Send flow: ctor(Proxy) -> NetSerialize
	FServerReplicationRPCParameter(FReplicationProxy& InProxy) : Proxy(&InProxy) { }
	bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess);

private:

	static TArray<uint8> TempStorage;

	FReplicationProxy* Proxy;
	int64 CachedNumBits = -1;
	class UPackageMap* CachedPackageMap = nullptr;
};

template<>
struct TStructOpsTypeTraits<FServerReplicationRPCParameter> : public TStructOpsTypeTraitsBase2<FServerReplicationRPCParameter>
{
	enum
	{
		WithNetSerializer = true
	};
};

// Helper struct to bypass the bandwidth limit imposed by the engine's NetDriver (QueuedBits, NetSpeed, etc).
// This is really a temp measure to make the system easier to drop in/try in a project without messing with your engine settings.
// (bandwidth optimizations have not been done yet and the system in general hasn't been stressed with packetloss / gaps in command streams)
// So, you are free to use this in your own code but it may be removed one day. Hopefully in general bandwidth limiting will also become more robust.
struct NETWORKPREDICTION_API FScopedBandwidthLimitBypass
{
	FScopedBandwidthLimitBypass(AActor* OwnerActor);
	~FScopedBandwidthLimitBypass();
private:

	int64 RestoreBits = 0;
	class UNetConnection* CachedNetConnection = nullptr;
};

