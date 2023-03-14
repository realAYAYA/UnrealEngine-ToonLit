// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Net/Core/NetBitArray.h"
#include "Iris/ReplicationSystem/Filtering/NetObjectFilter.h"
#include "Containers/Array.h"
#include "UObject/StrongObjectPtr.h"

class UReplicationSystem;
namespace UE::Net
{
	typedef uint32 FNetObjectFilterHandle;
	typedef uint16 FNetObjectGroupHandle;
	namespace Private
	{
		class FDeltaCompressionBaselineInvalidationTracker;
		class FNetHandleManager;
		class FNetObjectGroups;
		class FReplicationConnections;
	}
}

namespace UE::Net::Private
{

struct FReplicationFilteringInitParams
{
	TObjectPtr<UReplicationSystem> ReplicationSystem;
	const FNetHandleManager* NetHandleManager = nullptr;
	FNetObjectGroups* Groups = nullptr;
	FDeltaCompressionBaselineInvalidationTracker* BaselineInvalidationTracker = nullptr;
	FReplicationConnections* Connections = nullptr;
	uint32 MaxGroupCount = 0;
	uint32 MaxObjectCount = 0;
};

class FReplicationFiltering
{
public:
	FReplicationFiltering();

	void Init(FReplicationFilteringInitParams& Params);

	/**
	 * Determine which objects are allowed to be replicated to which connections.
	 * This is done for all connections every tick.
	 */
	void Filter(const FNetBitArrayView& DirtyObjects);

	FNetBitArrayView GetObjectsInScope(uint32 ConnectionId) const;

	FNetBitArrayView GetGroupFilteredOutObjects(uint32 ConnectionId) const;

	// Who owns what?
	void SetOwningConnection(uint32 ObjectIndex, uint32 ConnectionId);
	uint32 GetOwningConnection(uint32 ObjectIndex) const { return !bHasDirtyOwner ? ObjectIndexToOwningConnection[ObjectIndex] : GetOwningConnectionIfDirty(ObjectIndex); }

	// Various filters
	bool SetFilter(uint32 ObjectIndex, FNetObjectFilterHandle Filter);

	// Set whether an object is allowed to be replicated to certain connections or not.
	bool SetConnectionFilter(uint32 ObjectIndex, const FNetBitArrayView& ConnectionIndices, ENetFilterStatus ReplicationStatus);

	FNetObjectFilterHandle GetFilterHandle(const FName FilterName) const;
	UNetObjectFilter* GetFilter(const FName FilterName) const;

	// Connection handling
	void AddConnection(uint32 ConnectionId);
	void RemoveConnection(uint32 ConnectionId);

	// Group based filtering
	void AddGroupFilter(FNetObjectGroupHandle GroupHandle);
	void RemoveGroupFilter(FNetObjectGroupHandle GroupHandle);
	bool IsGroupFilterGroup(FNetObjectGroupHandle GroupHandle) const { return GroupHandle && FilterGroups.GetBit(GroupHandle); }

	void SetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, ENetFilterStatus ReplicationStatus);
	void SetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, const FNetBitArrayView& ConnectionsBitArray, ENetFilterStatus);
	void SetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, uint32 ConnectionId, ENetFilterStatus ReplicationStatus);
	bool GetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, uint32 ConnectionId, ENetFilterStatus& OutReplicationStatus) const;

	void NotifyObjectAddedToGroup(FNetObjectGroupHandle GroupHandle, uint32 ObjectIndex);
	void NotifyObjectRemovedFromGroup(FNetObjectGroupHandle GroupHandle, uint32 ObjectIndex);

	void NotifyAddedDependentObject(uint32 ObjectIndex);
	void NotifyRemovedDependentObject(uint32 ObjectIndex);
	
	// SubObjectFilter status
	void AddSubObjectFilter(FNetObjectGroupHandle GroupHandle);
	void RemoveSubObjectFilter(FNetObjectGroupHandle GroupHandle);
	bool IsSubObjectFilterGroup(FNetObjectGroupHandle GroupHandle) const { return GroupHandle && SubObjectFilterGroups.GetBit(GroupHandle); }

	void SetSubObjectFilterStatus(FNetObjectGroupHandle GroupHandle, ENetFilterStatus ReplicationStatus);
	void SetSubObjectFilterStatus(FNetObjectGroupHandle GroupHandle, const FNetBitArrayView& ConnectionsBitArray, ENetFilterStatus);
	void SetSubObjectFilterStatus(FNetObjectGroupHandle GroupHandle, uint32 ConnectionId, ENetFilterStatus ReplicationStatus);
	bool GetSubObjectFilterStatus(FNetObjectGroupHandle GroupHandle, uint32 ConnectionId, ENetFilterStatus& OutReplicationStatus) const;

private:
	struct FPerConnectionInfo
	{
		// Objects filtered depending on owning connection or user set connection filtering
		FNetBitArray ConnectionFilteredObjects;
		// Objects filtered out due to one or more groups it belongs to is filtered out
		FNetBitArray GroupFilteredOutObjects;
		// Connection and group filtering is assumed to happen seldom. Avoid recalculating from scratch every frame.
		FNetBitArray ObjectsInScopeBeforeDynamicFiltering;
		// Objects in scope after all kinds of filtering, including dynamic filtering, has been applied
		FNetBitArray ObjectsInScope;

		// Which objects are filtered out by dynamic filters.
		FNetBitArray DynamicFilteredOutObjects;
	};

	struct FPerObjectInfo
	{
		// Note: Array is likely larger than one element.
		uint32 ConnectionIds[1];
	};

	static constexpr uint32 UsedPerObjectInfoStorageGrowSize = 32; // 256 bytes, 1024 indices
	typedef uint16 PerObjectInfoIndexType;

	struct FPerGroupInfo
	{
		PerObjectInfoIndexType ConnectionStateIndex;
	};

	struct FFilterInfo
	{
		TStrongObjectPtr<UNetObjectFilter> Filter;
		FName Name;
		uint32 ObjectCount = 0;
		// Objects with this filter set.
		FNetBitArray FilteredObjects;
	};

private:
	class FUpdateDirtyObjectsBatchHelper;
	
	static void StaticChecks();

	void InitFilters();

	void InitNewConnections();
	void ResetRemovedConnections();
	void UpdateObjectsInScope();
	void UpdateOwnerAndConnectionFiltering();
	void UpdateGroupFiltering();
	void PreUpdateDynamicFiltering();
	void UpdateDynamicFiltering();
	void PostUpdateDynamicFiltering();
	void UpdateSubObjectFilters();

	bool HasDynamicFilters() const;

	// Helper to update and reset group filter effects if objects are removed from a filter or after a filter status change, returns true if the group filter was changed
	bool UpdateGroupFilterEffectsForObject(uint32 ObjectIndex, uint32 ConnectionId, const FNetBitArray& ScopableObjects);
	bool HasOwnerFilter(uint32 ObjectIndex) const;
	bool HasConnectionFilter(uint32 ObjectIndex) const;

	PerObjectInfoIndexType AllocPerObjectInfo();
	void FreePerObjectInfo(PerObjectInfoIndexType Index);

	FPerObjectInfo* AllocPerObjectInfoForObject(uint32 ObjectIndex);
	void FreePerObjectInfoForObject(uint32 ObjectIndex);
	
	FPerObjectInfo* GetPerObjectInfo(PerObjectInfoIndexType Index);
	const FPerObjectInfo* GetPerObjectInfo(PerObjectInfoIndexType Index) const;

	void SetPerObjectInfoFilterStatus(FPerObjectInfo& ObjectInfo, ENetFilterStatus ReplicationStatus);

	ENetFilterStatus GetConnectionFilterStatus(const FPerObjectInfo& ObjectInfo, uint32 ConnectionId) const;
	bool IsConnectionFilterStatusAllowedForAnyConnection(const FPerObjectInfo& ObjectInfo) const;
	void SetConnectionFilterStatus(FPerObjectInfo& ObjectInfo, uint32 ConnectionId, ENetFilterStatus ReplicationStatus);
	bool GetIsFilteredOutByAnyGroup(uint32 ObjectInternalIndex, uint32 ConnectionId) const;
	void InternalSetGroupFilterStatus(FNetObjectGroupHandle GroupHandle, uint32 ConnectionId, ENetFilterStatus ReplicationStatus);
	uint32 GetOwningConnectionIfDirty(uint32 ObjectIndex) const;

	void RemoveFromDynamicFilter(uint32 ObjectIndex, uint32 FilterIndex);

	void NotifyFiltersOfDirtyObjects(const FNetBitArrayView& DirtyObjects);
	void BatchNotifyFiltersOfDirtyObjects(FUpdateDirtyObjectsBatchHelper& BatchHelper, const uint32* ObjectIndices, uint32 ObjectCount);

	void InvalidateBaselinesForObject(uint32 ObjectIndex, uint32 NewOwningConnectionId, uint32 PrevOwningConnectionId);

private:
	// Used for ObjectIndexToDynamicFilterIndex lookup
	static constexpr uint8 InvalidDynamicFilterIndex = 255U;

	// General
	TObjectPtr<UReplicationSystem> ReplicationSystem;
	const FNetHandleManager* NetHandleManager = nullptr;

	// Groups
	FNetObjectGroups* Groups = nullptr;

	// Baseline invalidation tracker
	FDeltaCompressionBaselineInvalidationTracker* BaselineInvalidationTracker = nullptr;

	// Connection specifics
	FReplicationConnections* Connections = nullptr;
	TArray<FPerConnectionInfo> ConnectionInfos;
	FNetBitArray ValidConnections;
	FNetBitArray NewConnections;

	// Object specifics
	uint32 MaxObjectCount = 0;
	uint32 WordCountForObjectBitArrays = 0;

	// Filter specifics
	FNetBitArray ObjectsWithDirtyConnectionFilter;
	FNetBitArray ObjectsWithDirtyOwner;

	FNetBitArray ObjectsWithOwnerFilter;
	TArray<uint16> ObjectIndexToOwningConnection;

	// For non-owner filtered objects
	// The storage for PerObjectInfo.
	TArray<uint32> PerObjectInfoStorage;
	// Storage for bit array indicating used/free status of PerObjectInfos
	TArray<uint32> UsedPerObjectInfoStorage;

	FNetBitArray ObjectsWithPerObjectInfo;

	// Groups
	TArray<FPerGroupInfo> GroupInfos;
	uint32 MaxGroupCount = 0;
	FNetBitArray FilterGroups;
	FNetBitArray DirtyFilterGroups;
	FNetBitArray SubObjectFilterGroups;
	FNetBitArray DirtySubObjectFilterGroups;

	TArray<PerObjectInfoIndexType> ObjectIndexToPerObjectInfoIndex;
	uint32 PerObjectInfoStorageCountForConnections = 0;
	// How many elements from UsedPerObjectInfoStorage is needed to hold a FPerObjectInfo
	uint32 PerObjectInfoStorageCountPerItem = 0;

	// Dynamic filters 
	TArray<FNetObjectFilteringInfo> NetObjectFilteringInfos;
	TArray<uint8> ObjectIndexToDynamicFilterIndex;
	TArray<FFilterInfo> DynamicFilterInfos;
	FNetBitArray DynamicFilterEnabledObjects;
	FNetBitArray ObjectsRequiringDynamicFilterUpdate;

	uint32 bHasNewConnection : 1;
	uint32 bHasRemovedConnection : 1;
	uint32 bHasDirtyConnectionFilter: 1;
	uint32 bHasDirtyOwner : 1;
};

inline bool FReplicationFiltering::HasDynamicFilters() const
{
	return DynamicFilterInfos.Num() > 0;
}

}
