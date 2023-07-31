// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "Iris/ReplicationSystem/NetHandle.h"

class UReplicationSystem;
namespace UE::Net
{
struct FReplicationProtocol;
struct FReplicationInstanceProtocol;
}

#if !UE_BUILD_SHIPPING

namespace UE::Net::IrisDebugHelper
{ 

/** Dummy methods binding pointers to external methods to avoid them from being stripped by compiler */
IRISCORE_API uint64 Init();

/** Trigger a breakpoint and return true if the object contains the current debug name */
bool BreakOnObjectName(UObject* Object);

/** Trigger a breakpoint and return true if the NetHandle is the current debug nethandle */
bool BreakOnNetHandle(FNetHandle NetHandle);

/** Trigger a breakpoint and return true if the name contains the debug RPC string */
bool BreakOnRPCName(FName RPCName);

/** Output state data to StringBuilder for the specified Handle */
void NetObjectStateToString(FStringBuilderBase& StringBuilder, FNetHandle NetHandle);

/** Find all handles references registered for a protocol and output to StringBuilder */
void NetObjectProtocolReferencesToString(FStringBuilderBase& StringBuilder, uint64 ProtocolId, uint32 ReplicationSystemId);

// Helper functions for debugging state data exposed as extern "C" that are callable from immediate and watch window in the debugger

/** Get the ReplicationSystem with the Id */
extern "C" IRISCORE_API UReplicationSystem* GetReplicationSystemForDebug(uint32 Id);

/** DebugOutputObject state for Handle specified by NetHandleId and ReplicationSystemId and */
extern "C" IRISCORE_API void DebugOutputNetObjectState(uint32 NetHandleId, uint32 ReplicationSystemId);

/**
	Variant of NetObjectStateToString that can be used from breakpoints and in watch window to print the current state of a NetHandle
	NOTE: Use only for debugging as this variant uses a static buffer which is not thread safe	
*/
extern "C" IRISCORE_API const TCHAR* DebugNetObjectStateToString(uint32 NetHandleId, uint32 ReplicationSystemId);

/** Find all handles references registered for a protocol and output to DebugOutput in debugger */
extern "C" IRISCORE_API void DebugOutputNetObjectProtocolReferences(uint64 ProtocolId, uint32 ReplicationSystemId);

/** Get info about replicated object */
struct FNetReplicatedObjectDebugInfo
{
	const FNetHandle* Handle;
	uint32 InternalNetHandleIndex;
	const UReplicationSystem* ReplicationSystem;
	const FReplicationProtocol* Protocol;
	const FReplicationInstanceProtocol* InstanceProtocol;
};

/** Look up replicated NetHandle from Instance pointer and return debug information, this variant searches all active replication systems */
extern "C" IRISCORE_API FNetReplicatedObjectDebugInfo DebugNetObject(UObject* Instance);

/** Look up replicated NetHandle from Instance pointer and return debug information, this variant only searches the replicatioSystem with the specified id */
extern "C" IRISCORE_API FNetReplicatedObjectDebugInfo DebugNetObjectById(UObject* Instance, uint32 ReplicationSystemId);

/** Look up replicated handle and return debug information */
extern "C" IRISCORE_API FNetReplicatedObjectDebugInfo DebugNetHandle(FNetHandle Handle);

/** Look up replicated handle specified by handle id and replicationsystem id and return debug information */
extern "C" IRISCORE_API FNetReplicatedObjectDebugInfo DebugNetHandleById(uint32 NetHandleId, uint32 ReplicationSystemId);

/** Variant of DebugOutputNetObjectProtocolReferences that can be used from breakpoints or in watch window to find all handles references registered for a protocol and output to DebugOutput in debugger
 * NOTE: Use only for debugging as this variant uses a static buffer which is not thread safe	
*/
extern "C" IRISCORE_API const TCHAR* DebugNetObjectProtocolReferencesToString(uint64 ProtocolId, uint32 ReplicationSystemId);

/** Set the Object Name to break on */
extern "C" IRISCORE_API void SetIrisDebugObjectName(const ANSICHAR* NameBuffer);

/** Set the NetHandle to break on */
extern "C" IRISCORE_API void SetIrisDebugNetHandle(uint32 NetHandleId);

/** Set the RPC Name to break on */
extern "C" IRISCORE_API void SetIrisDebugRPCName(const ANSICHAR* NameBuffer);

}; // end of namespaces

#endif

