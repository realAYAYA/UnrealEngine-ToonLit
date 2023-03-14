// Copyright Epic Games, Inc. All Rights Reserved.

#include "Iris/Core/IrisDebugging.h"

#if !UE_BUILD_SHIPPING

#include "UObject/Field.h"
#include "HAL/IConsoleManager.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Iris/Core/IrisLog.h"
#include "Iris/ReplicationSystem/ObjectReplicationBridge.h"
#include "Iris/ReplicationSystem/ReplicationOperations.h"
#include "Iris/ReplicationSystem/ReplicationSystem.h"
#include "Iris/ReplicationSystem/ReplicationSystemInternal.h"
#include "Iris/ReplicationSystem/NetHandleManager.h"
#include "Iris/ReplicationSystem/ReplicationProtocolManager.h"
#include "Iris/ReplicationState/PropertyReplicationState.h"
#include "Iris/Serialization/NetSerializationContext.h"
#include "Iris/Serialization/InternalNetSerializationContext.h"
#include "Containers/StringFwd.h"

namespace UE::Net::IrisDebugHelper
{

namespace IrisDebugHelperInternal
{
	static FString GIrisDebugName;
	FAutoConsoleVariableRef NetIrisDebugName(
		TEXT("Net.Iris.DebugName"),
		GIrisDebugName,
		TEXT("Set a class name or object name to break on."),
		ECVF_Default);

	static FString GIrisDebugRPCName;
	FAutoConsoleVariableRef NetRPCSetDebugRPCName(
		TEXT("Net.Iris.DebugRPCName"),
		GIrisDebugRPCName,
		TEXT("Set the Name of an RPC to break on."),
		ECVF_Default);

	static FNetHandle GIrisDebugNetHandle;
	static FAutoConsoleCommand NetIrisDebugNetHandle(
		TEXT("Net.Iris.DebugNetHandle"), 
		TEXT("Specify an handle index that we will break on (or none to turn off)."), 
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() > 0)
			{
				uint32 NetId = 0;
				LexFromString(NetId, *Args[0]);
				GIrisDebugNetHandle = Private::FNetHandleManager::MakeNetHandleFromId(NetId);
			}
			else
			{
				GIrisDebugNetHandle = FNetHandle();
			}
		}));
}; // namespace IrisDebugHelperInternal

bool BreakOnObjectName(UObject* Object)
{
	if (IrisDebugHelperInternal::GIrisDebugName.IsEmpty() == false && GetNameSafe(Object).Contains(IrisDebugHelperInternal::GIrisDebugName))
	{
		UE_DEBUG_BREAK();
		return true;
	}

	return false;
}

bool BreakOnNetHandle(FNetHandle NetHandle)
{
	if (IrisDebugHelperInternal::GIrisDebugNetHandle.IsValid() && IrisDebugHelperInternal::GIrisDebugNetHandle == NetHandle)
	{
		UE_DEBUG_BREAK();
		return true;
	}

	return false;
}

bool BreakOnRPCName(FName RPCName)
{
	if (IrisDebugHelperInternal::GIrisDebugRPCName.IsEmpty() == false && RPCName.GetPlainNameString().Contains(IrisDebugHelperInternal::GIrisDebugRPCName))
	{
		UE_DEBUG_BREAK();
		return true;
	}

	return false;
}

void SetIrisDebugObjectName(const ANSICHAR* NameBuffer)
{
	if (NameBuffer)
	{
		IrisDebugHelperInternal::GIrisDebugName = ANSI_TO_TCHAR(NameBuffer);
	}
	else
	{
		IrisDebugHelperInternal::GIrisDebugName = FString();
	}
}

void SetIrisDebugNetHandle(uint32 NetHandleId)
{
	IrisDebugHelperInternal::GIrisDebugNetHandle = Private::FNetHandleManager::MakeNetHandleFromId(NetHandleId);
}

void SetIrisDebugRPCName(const ANSICHAR* NameBuffer)
{
	if (NameBuffer)
	{
		IrisDebugHelperInternal::GIrisDebugRPCName = ANSI_TO_TCHAR(NameBuffer);
	}
	else
	{
		IrisDebugHelperInternal::GIrisDebugRPCName = FString();
	}
}

UReplicationSystem* GetReplicationSystemForDebug(uint32 Id)
{
	return GetReplicationSystem(Id);
}

#define UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(FunctionName) FunctionReferenceAccumulator += uint64(&FunctionName);

uint64 Init()
{
	uint64 FunctionReferenceAccumulator = uint64(0);

	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(GetReplicationSystemForDebug);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(DebugOutputNetObjectState);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(DebugNetObjectStateToString);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(DebugOutputNetObjectProtocolReferences);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(DebugNetObject);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(DebugNetObjectById);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(DebugNetHandle);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(DebugNetHandleById);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(DebugNetObjectProtocolReferencesToString);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(SetIrisDebugObjectName);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(SetIrisDebugNetHandle);
	UE_NET_FORCE_REFERENCE_DEBUGFUNCTION(SetIrisDebugRPCName);
	
	return FunctionReferenceAccumulator;
}

#undef UE_NET_FORCE_REFERENCE_DEBUGFUNCTION

void NetObjectStateToString(FStringBuilderBase& StringBuilder, FNetHandle NetHandle)
{
	using namespace UE::Net::Private;

	if (!NetHandle.IsValid())
	{
		return;
	}

	UReplicationSystem* ReplicationSystem = GetReplicationSystem(NetHandle.GetReplicationSystemId());
	if (!ReplicationSystem)
	{
		return;
	}

	FReplicationSystemInternal* ReplicationSystemInternal = ReplicationSystem->GetReplicationSystemInternal();
	const FNetHandleManager& NetHandleManager = ReplicationSystemInternal->GetNetHandleManager();

	const uint32 NetObjectInternalIndex = NetHandleManager.GetInternalIndex(NetHandle);
	if (!NetObjectInternalIndex)
	{
		return;
	}

	const bool bIsServer = ReplicationSystem->IsServer();
	const FNetHandleManager::FReplicatedObjectData& ReplicatedObjectData = NetHandleManager.GetReplicatedObjectDataNoCheck(NetObjectInternalIndex);	
	const uint8* InternalStateBuffer = bIsServer ? NetHandleManager.GetReplicatedObjectStateBufferNoCheck(NetObjectInternalIndex) : ReplicatedObjectData.ReceiveStateBuffer;
	if (ReplicatedObjectData.InstanceProtocol == nullptr || InternalStateBuffer == nullptr)
	{
		return;
	}

	const FReplicationProtocol* Protocol = ReplicatedObjectData.Protocol;

	// In order to be able to output object references we need the TokenStoreState, for the server we just use the local one but if we are a client we must use the remote token store state
	// If this is a client handle we assume that we only have a single connections and use the first valid connection to get the remote token store.
	FReplicationConnections& Connections = ReplicationSystemInternal->GetConnections();
	const uint32 FirstValidConnectionId = Connections.GetValidConnections().FindFirstOne();
	FNetTokenStoreState* TokenStoreState = (bIsServer || (FirstValidConnectionId == FNetBitArray::InvalidIndex)) ? ReplicationSystemInternal->GetNetTokenStore().GetLocalNetTokenStoreState() : &Connections.GetRemoteNetTokenStoreState(FirstValidConnectionId);

	// Setup Context
	FInternalNetSerializationContext InternalContext;
	FInternalNetSerializationContext::FInitParameters InternalContextInitParams;
	InternalContextInitParams.ReplicationSystem = ReplicationSystem;
	InternalContextInitParams.ObjectResolveContext.RemoteNetTokenStoreState = TokenStoreState;
	InternalContextInitParams.ObjectResolveContext.ConnectionId = (FirstValidConnectionId == FNetBitArray::InvalidIndex ? InvalidConnectionId : FirstValidConnectionId);
	InternalContext.Init(InternalContextInitParams);

	FNetSerializationContext NetSerializationContext;
	NetSerializationContext.SetInternalContext(&InternalContext);
	NetSerializationContext.SetLocalConnectionId(InternalContextInitParams.ObjectResolveContext.ConnectionId);

	StringBuilder << TEXT("State for ") << NetHandle;
	StringBuilder.Appendf(TEXT(" of type %s\n"), Protocol->DebugName->Name);

	FReplicationInstanceOperations::OutputInternalStateToString(NetSerializationContext, StringBuilder, nullptr, InternalStateBuffer, ReplicatedObjectData.InstanceProtocol, ReplicatedObjectData.Protocol);
}

void DebugOutputNetObjectState(uint32 NetHandleId, uint32 ReplicationSystemId)
{
	const FNetHandle NetHandle = Private::FNetHandleManager::MakeNetHandle(NetHandleId, ReplicationSystemId);

	TStringBuilder<4096> StringBuilder;

	NetObjectStateToString(StringBuilder, NetHandle);

	FPlatformMisc::LowLevelOutputDebugString(StringBuilder.ToString());
}

const TCHAR* DebugNetObjectStateToString(uint32 NetHandleId, uint32 ReplicationSystemId)
{
	static TStringBuilder<4096> StringBuilder;

	StringBuilder.Reset();
	const FNetHandle NetHandle = Private::FNetHandleManager::MakeNetHandle(NetHandleId, ReplicationSystemId);
	NetObjectStateToString(StringBuilder, NetHandle);

	return StringBuilder.ToString();
}

FNetReplicatedObjectDebugInfo DebugNetObject(UObject* Instance)
{
	using namespace UE::Net::Private;
	FNetReplicatedObjectDebugInfo Info = {};

	// See if we can find the instance in any replication system
	for (uint32 RepSystemIt = 0; RepSystemIt < FReplicationSystemFactory::MaxReplicationSystemCount; ++RepSystemIt)
	{
		const UReplicationSystem* ReplicationSystem = GetReplicationSystemForDebug(RepSystemIt);
		if (ReplicationSystem)
		{
			if (const UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
			{
				FNetHandle Handle = Bridge->GetReplicatedHandle(Instance);
				if (Handle.IsValid())
				{
					const FNetHandleManager& NetHandleManager = ReplicationSystem->GetReplicationSystemInternal()->GetNetHandleManager();
					Info.InternalNetHandleIndex = NetHandleManager.GetInternalIndex(Handle);

					const FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager.GetReplicatedObjectDataNoCheck(Info.InternalNetHandleIndex);				

					Info.Handle = &ObjectData.Handle;
					Info.Protocol = ObjectData.Protocol;
					Info.InstanceProtocol = ObjectData.InstanceProtocol;
					Info.ReplicationSystem = ReplicationSystem;

					return Info;
				}
			}
		}
	}

	return Info;
}

FNetReplicatedObjectDebugInfo DebugNetObjectById(UObject* Instance, uint32 ReplicationSystemId)
{
	using namespace UE::Net::Private;
	FNetReplicatedObjectDebugInfo Info = {};

	const UReplicationSystem* ReplicationSystem = GetReplicationSystemForDebug(ReplicationSystemId);
	if (ReplicationSystem)
	{
		if (const UObjectReplicationBridge* Bridge = ReplicationSystem->GetReplicationBridgeAs<UObjectReplicationBridge>())
		{
			FNetHandle Handle = Bridge->GetReplicatedHandle(Instance);
			if (Handle.IsValid())
			{
				const FNetHandleManager& NetHandleManager = ReplicationSystem->GetReplicationSystemInternal()->GetNetHandleManager();
				Info.InternalNetHandleIndex = NetHandleManager.GetInternalIndex(Handle);

				const FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager.GetReplicatedObjectDataNoCheck(Info.InternalNetHandleIndex);

				Info.Handle = &ObjectData.Handle;
				Info.Protocol = ObjectData.Protocol;
				Info.InstanceProtocol = ObjectData.InstanceProtocol;
				Info.ReplicationSystem = ReplicationSystem;

				return Info;
			}
		}
	}

	return Info;
}

FNetReplicatedObjectDebugInfo DebugNetHandle(FNetHandle Handle)
{
	return DebugNetHandleById(Handle.GetId(), Handle.GetReplicationSystemId());
}

FNetReplicatedObjectDebugInfo DebugNetHandleById(uint32 NetHandleId, uint32 ReplicationSystemId)
{
	using namespace UE::Net::Private;
	FNetReplicatedObjectDebugInfo Info = {};

	// See if we can find the instance in any replication system
	{
		const UReplicationSystem* ReplicationSystem = GetReplicationSystemForDebug(ReplicationSystemId);
		if (ReplicationSystem)
		{
			const FNetHandleManager& NetHandleManager = ReplicationSystem->GetReplicationSystemInternal()->GetNetHandleManager();
			FNetHandle IncompleteHandle = FNetHandleManager::MakeNetHandle(NetHandleId, ReplicationSystemId);
			const uint32 InternalNetHandleIndex = Info.InternalNetHandleIndex = NetHandleManager.GetInternalIndex(IncompleteHandle);
			if (InternalNetHandleIndex != FNetHandleManager::InvalidInternalIndex)
			{
				const FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager.GetReplicatedObjectDataNoCheck(InternalNetHandleIndex);
				if (ObjectData.Handle == IncompleteHandle)
				{
					Info.Handle = &ObjectData.Handle;
					Info.InternalNetHandleIndex = InternalNetHandleIndex;
					Info.Protocol = ObjectData.Protocol;
					Info.InstanceProtocol = ObjectData.InstanceProtocol;
					Info.ReplicationSystem = ReplicationSystem;

					return Info;
				}
			}
		}
	}

	return Info;
}

void NetObjectProtocolReferencesToString(FStringBuilderBase& StringBuilder, uint64 ProtocolId, uint32 ReplicationSystemId)
{
	using namespace UE::Net::Private;

	UReplicationSystem* ReplicationSystem = GetReplicationSystem(ReplicationSystemId);
	if (!ReplicationSystem)
	{
		return;
	}

	FReplicationSystemInternal* ReplicationSystemInternal = ReplicationSystem->GetReplicationSystemInternal();
	const FNetHandleManager& NetHandleManager = ReplicationSystemInternal->GetNetHandleManager();
	const FReplicationProtocolManager& ProtocolManager = ReplicationSystemInternal->GetReplicationProtocolManager();

	// As there might be multiple protocols sharing the same identifier
	ProtocolManager.ForEachProtocol(ProtocolId, [&StringBuilder, &NetHandleManager](const FReplicationProtocol* Protocol, const UObject* ArchetypeOrCDOUsedAsKey)
		{
			StringBuilder << TEXT("Protocol: ") << ToCStr(Protocol->DebugName);
			StringBuilder.Appendf(TEXT("Id: 0x%" UINT64_x_FMT " Created From : 0x%p"), Protocol->ProtocolIdentifier, ArchetypeOrCDOUsedAsKey) << TEXT(" Used by : \n");

			auto FindMatchingProtocol = [&StringBuilder, &Protocol, &NetHandleManager](uint32 InternalObjectIndex)
			{
				const FNetHandleManager::FReplicatedObjectData& ObjectData = NetHandleManager.GetReplicatedObjectDataNoCheck(InternalObjectIndex);
				if (ObjectData.Protocol == Protocol)
				{
					StringBuilder << ObjectData.Handle << TEXT(" ( InternalIndex: ") << InternalObjectIndex << TEXT(", )\n");
				}
			};
			
			NetHandleManager.GetAssignedInternalIndices().ForAllSetBits(FindMatchingProtocol);
		}
	);
}

void DebugOutputNetObjectProtocolReferences(uint64 ProtocolId, uint32 ReplicationSystemId)
{
	TStringBuilder<4096> StringBuilder;
	StringBuilder.Reset();

	NetObjectProtocolReferencesToString(StringBuilder, ProtocolId, ReplicationSystemId);

	FPlatformMisc::LowLevelOutputDebugString(StringBuilder.ToString());
}

const TCHAR* DebugNetObjectProtocolReferencesToString(uint64 ProtocolId, uint32 ReplicationSystemId)
{
	static TStringBuilder<4096> StringBuilder;
	StringBuilder.Reset();

	NetObjectProtocolReferencesToString(StringBuilder, ProtocolId, ReplicationSystemId);

	return StringBuilder.ToString();
}

} // End of namespaces

#endif