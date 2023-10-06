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
		class FNetRefHandleManager;
		class FNetObjectGroups;
		class FReplicationConnections;
		typedef uint32 FInternalNetRefIndex;
	}
}

namespace UE::Net::Private
{

class FNetObjectFilteringInfoAccessor
{
private:
	/** Returns all the NetObjectFilteringInfos for the filtering system. */
	TArrayView<FNetObjectFilteringInfo> GetNetObjectFilteringInfos(UReplicationSystem* ReplicationSystem) const;

private:
	// Friends
	friend UNetObjectFilter;
};


struct FReplicationFilteringInitParams
{
	TObjectPtr<UReplicationSystem> ReplicationSystem;
	const FNetRefHandleManager* NetRefHandleManager = nullptr;
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
	 * First pass to determine which objects are relevant to each connection.
	 * Executes group, owner and connection filtering then any raw dynamic filters.
	 * At the end any object that is not relevant to at least one connection will be removed from the scoped object list.
	 * Exception to this rule are always relevant (e.g. non-filtered) objects or objects set to be filtered by a fragment-based dynamic filter.
	 */
	void FilterPrePoll();

	/**
	 * Second pass to determine which objects are relevant to each connection.
	 * Executes only fragment-based dynamic filters.
	 */
	void FilterPostPoll();

	/** 
	* Returns the list of objects relevant to a given connection.
	* This represents the global scope list minus the objects that were filtered out for the given connection.
	*/
	const FNetBitArrayView GetRelevantObjectsInScope(uint32 ConnectionId) const
	{
		return MakeNetBitArrayView(ConnectionInfos[ConnectionId].ObjectsInScope);
	}

	const FNetBitArrayView GetGroupFilteredOutObjects(uint32 ConnectionId) const
	{
		return MakeNetBitArrayView(ConnectionInfos[ConnectionId].GroupFilteredOutObjects);
	}

	// Who owns what?
	void SetOwningConnection(FInternalNetRefIndex ObjectIndex, uint32 ConnectionId);
	uint32 GetOwningConnection(FInternalNetRefIndex ObjectIndex) const { return !bHasDirtyOwner ? ObjectIndexToOwningConnection[ObjectIndex] : GetOwningConnectionIfDirty(ObjectIndex); }

	// Various filters
	bool SetFilter(FInternalNetRefIndex ObjectIndex, FNetObjectFilterHandle Filter);

	// Set whether an object is allowed to be replicated to certain connections or not.
	bool SetConnectionFilter(FInternalNetRefIndex ObjectIndex, const FNetBitArrayView& ConnectionIndices, ENetFilterStatus ReplicationStatus);

	FNetObjectFilterHandle GetFilterHandle(const FName FilterName) const;
	UNetObjectFilter* GetFilter(const FName FilterName) const;

	/** Returns the name of the Filter represented by the handle. */
	FName GetFilterName(FNetObjectFilterHandle Filter) const;

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

	void NotifyObjectAddedToGroup(FNetObjectGroupHandle GroupHandle, FInternalNetRefIndex ObjectIndex);
	void NotifyObjectRemovedFromGroup(FNetObjectGroupHandle GroupHandle, FInternalNetRefIndex ObjectIndex);

	void NotifyAddedDependentObject(FInternalNetRefIndex ObjectIndex);
	void NotifyRemovedDependentObject(FInternalNetRefIndex ObjectIndex);
	
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

		// List of objects currently filtered out by a either dynamic filter passes
		FNetBitArray InProgressDynamicFilteredOutObjects;
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
		ENetFilterType Type = ENetFilterType::PrePoll_Raw;
		TStrongObjectPtr<UNetObjectFilter> Filter;
		FName Name;
		uint32 ObjectCount = 0;
		// Objects with this filter set.
		FNetBitArray FilteredObjects;
	};

private:
	class FUpdateDirtyObjectsBatchHelper;
	friend FNetObjectFilteringInfoAccessor;
	
	static void StaticChecks();

	void InitFilters();

	void InitNewConnections();
	void ResetRemovedConnections();
	void UpdateObjectsInScope();
	void UpdateOwnerAndConnectionFiltering();
	void UpdateGroupFiltering();
	void UpdateSubObjectFilters();

	void UpdateDynamicFilters(ENetFilterType FilterPass);
	void PreUpdateDynamicFiltering(ENetFilterType FilterType);
	void UpdateDynamicFiltering(ENetFilterType FilterType);
	void PostUpdateDynamicFiltering(ENetFilterType FilterType);

	/** Build the list of always relevant objects + objects that are currently relevant to at least one connection. */
	void FilterNonRelevantObjects();

	bool HasDynamicFilters() const;
	bool HasRawFilters() const;
	bool HasFragmentFilters() const;

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

	void NotifyFiltersOfDirtyObjects(ENetFilterType FilterType);
	void BatchNotifyFiltersOfDirtyObjects(FUpdateDirtyObjectsBatchHelper& BatchHelper, const uint32* ObjectIndices, uint32 ObjectCount);

	void InvalidateBaselinesForObject(uint32 ObjectIndex, uint32 NewOwningConnectionId, uint32 PrevOwningConnectionId);

	/** Returns all the filtering infos. */
	TArrayView<FNetObjectFilteringInfo> GetNetObjectFilteringInfos();

private:
	// Used for ObjectIndexToDynamicFilterIndex lookup
	static constexpr uint8 InvalidDynamicFilterIndex = 255U;

	// General
	TObjectPtr<UReplicationSystem> ReplicationSystem;
	const FNetRefHandleManager* NetRefHandleManager = nullptr;

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

	/** NetObjectGroups who have the group filtering trait enabled */
	FNetBitArray FilterGroups;

	/** NetObjectGroups who have dirty members and their filtering updated this frame */
	FNetBitArray DirtyFilterGroups;

	//$IRIS TODO: These need better documentation
	FNetBitArray SubObjectFilterGroups;
	FNetBitArray DirtySubObjectFilterGroups;
	FNetBitArray AllConnectionFilteredObjects;

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
	uint32 bHasDynamicFilters : 1;
	uint32 bHasDynamicRawFilters : 1;
	uint32 bHasDynamicFragmentFilters : 1;
};

inline bool FReplicationFiltering::HasDynamicFilters() const
{
	return bHasDynamicFilters;
}

inline bool FReplicationFiltering::HasRawFilters() const
{
	return bHasDynamicRawFilters;
}
inline bool FReplicationFiltering::HasFragmentFilters() const
{
	return bHasDynamicFragmentFilters;
}

}
