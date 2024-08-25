// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Net/Core/NetBitArray.h"

enum ELifetimeCondition : int;
namespace UE::Net
{
	enum class EReplicationCondition : uint32;
	struct FReplicationProtocol;
	namespace Private
	{
		class FDeltaCompressionBaselineInvalidationTracker;
		typedef uint32 FInternalNetRefIndex;
		class FNetRefHandleManager;
		class FReplicationConnections;
		class FReplicationFiltering;
		class FNetObjectGroups;
	}
}

namespace UE::Net::Private
{

struct FReplicationConditionalsInitParams
{
	const FNetRefHandleManager* NetRefHandleManager = nullptr;
	const FReplicationFiltering* ReplicationFiltering = nullptr;
	const FReplicationConnections* ReplicationConnections = nullptr;
	const FNetObjectGroups* NetObjectGroups = nullptr;
	FDeltaCompressionBaselineInvalidationTracker* BaselineInvalidationTracker = nullptr;
	uint32 MaxObjectCount = 0;
	uint32 MaxConnectionCount = 0;
};

/**
 * NOTE: Almost everything in this class is for backward compatibility mode only. As such some functions are
 * fairly slow. For declared replication states this overhead will be eliminated because the state
 * itself can do all the relevant tracking locally. The one thing that will possibly remain is the unmasking
 * of changemasks. The unmasking could then either be done on the sending side, CPU vs bandwidth trade-off,
 * or on the receiving side, not so CPU costly as there's typically one connection and maybe tens of objects.
 * The unmasking is then likely to move to protocol operations.
 */
class FReplicationConditionals
{
public:
	FReplicationConditionals();

	void Init(FReplicationConditionalsInitParams& Params);

	void AddConnection(uint32 ConnectionId);
	void RemoveConnection(uint32 ConnectionId);

	bool SetConditionConnectionFilter(FInternalNetRefIndex ObjectIndex, EReplicationCondition Condition, uint32 ConnectionId, bool bEnable);
	bool SetCondition(FInternalNetRefIndex ObjectIndex, EReplicationCondition Condition, bool bEnable);

	// For property custom conditions only
	void InitPropertyCustomConditions(FInternalNetRefIndex ObjectIndex);
	bool SetPropertyCustomCondition(FInternalNetRefIndex ObjectIndex, const void* Owner, uint16 RepIndex, bool bIsActive);
	bool SetPropertyDynamicCondition(FInternalNetRefIndex ObjectIndex, const void* Owner, uint16 RepIndex, ELifetimeCondition Condition);
	
	/** Unconditionally marks a property as dirty, causing it to replicate with the object at the earliest convenience. */
	void MarkPropertyDirty(FInternalNetRefIndex ObjectIndex, uint16 RepIndex);

	void Update();

	bool ApplyConditionalsToChangeMask(uint32 ReplicatingConnectionId, bool bIsInitialState, FInternalNetRefIndex ParentObjectIndex, FInternalNetRefIndex ObjectIndex, uint32* ChangeMaskData, const uint32* ConditionalChangeMaskData, const FReplicationProtocol* Protocol);

	using FSubObjectsToReplicateArray = TArray<FInternalNetRefIndex, TInlineAllocator<32>>;
	void GetSubObjectsToReplicate(uint32 ReplicationConnectionId, FInternalNetRefIndex ParentObjectIndex, FSubObjectsToReplicateArray& OutSubObjectsToReplicate);

private:
	struct FPerObjectInfo
	{
		// Assume there can only be one connection which has the role autonomous connection and all else are simulated
		uint16 AutonomousConnectionId : 15;
		uint16 bRepPhysics : 1;
	};

	struct FConditionalsMask
	{
		bool IsUninitialized() const { return ConditionalsMask == 0; }
		bool IsConditionEnabled(int Condition) const { return ConditionalsMask & (uint16(1) << unsigned(Condition)); }
		bool SetConditionEnabled(int Condition, bool bEnabled) { return ConditionalsMask |= (uint16(bEnabled ? 1 : 0) << unsigned(Condition)); }

		// Each LifetimeCondition is represented in this member via (1U << ELifetimeCondition)
		uint16 ConditionalsMask;
	};

	struct FPerConnectionInfo
	{
		TArray<FConditionalsMask> ObjectConditionals;
	};

	struct FSubObjectConditionInfo
	{
		// This is a more compact storage form of ELifetimeCondition
		int8 Condition;
	};

	struct FObjectDynamicConditions
	{
		// RepIndex and ELifetimeCondition expressed as int16
		TMap<uint16, int16> DynamicConditions;
	};

private:
	void UpdateObjectsInScope();

	FConditionalsMask GetLifetimeConditionals(uint32 ReplicatingConnectionId, FInternalNetRefIndex ParentObjectIndex, bool bInitialState) const;

	FPerObjectInfo* GetPerObjectInfo(FInternalNetRefIndex ObjectIndex);
	const FPerObjectInfo* GetPerObjectInfo(FInternalNetRefIndex ObjectIndex) const;
	void ClearPerObjectInfo(FInternalNetRefIndex ObjectIndex);
	void ClearConnectionInfosForObject(const FNetBitArray& ValidConnections, uint32 MaxConnectionId, FInternalNetRefIndex ObjectIndex);

	void GetChildSubObjectsToReplicate(uint32 ReplicatingConnectionId, const FConditionalsMask& LifetimeConditionals, const FInternalNetRefIndex ParentObjectIndex, FSubObjectsToReplicateArray& OutSubObjectsToReplicate);

	ELifetimeCondition GetDynamicCondition(FInternalNetRefIndex ObjectIndex, uint16 RepIndex) const;
	void SetDynamicCondition(FInternalNetRefIndex ObjectIndex, uint16 RepIndex, ELifetimeCondition Condition);
	bool DynamicConditionChangeRequiresBaselineInvalidation(ELifetimeCondition OldCondition, ELifetimeCondition NewCondition) const;

	void MarkRemoteRoleDirty(FInternalNetRefIndex ObjectIndex);
	uint16 GetRemoteRoleRepIndex(const FReplicationProtocol* Protocol);

private:
	static constexpr uint16 InvalidRepIndex = 65535U;

	const FNetRefHandleManager* NetRefHandleManager = nullptr;
	const FReplicationFiltering* ReplicationFiltering = nullptr;
	const FReplicationConnections* ReplicationConnections = nullptr;
	FDeltaCompressionBaselineInvalidationTracker* BaselineInvalidationTracker = nullptr;
	const FNetObjectGroups* NetObjectGroups = nullptr;

	TArray<FPerObjectInfo> PerObjectInfos;
	TArray<FPerConnectionInfo> ConnectionInfos;
	TMap<FInternalNetRefIndex, FObjectDynamicConditions> DynamicConditions;

	uint32 MaxObjectCount = 0;
	uint32 MaxConnectionCount = 0;
	uint16 CachedRemoteRoleRepIndex = InvalidRepIndex;
};

inline FReplicationConditionals::FPerObjectInfo* FReplicationConditionals::GetPerObjectInfo(FInternalNetRefIndex ObjectIndex)
{
	return PerObjectInfos.GetData() + ObjectIndex;
}

inline const FReplicationConditionals::FPerObjectInfo* FReplicationConditionals::GetPerObjectInfo(FInternalNetRefIndex ObjectIndex) const
{
	return PerObjectInfos.GetData() + ObjectIndex;
}

}
