// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Net/Core/NetBitArray.h"
#include "Iris/ReplicationSystem/ReplicationView.h"
#include "UObject/ObjectMacros.h"
#include "NetObjectFilter.generated.h"

struct FNetObjectFilteringInfo;
class UReplicationSystem;
namespace UE::Net
{
	typedef uint32 FNetObjectFilterHandle;
	typedef uint16 FNetObjectGroupHandle;
	struct FReplicationInstanceProtocol;
	struct FReplicationProtocol;
}

namespace UE::Net
{

constexpr FNetObjectFilterHandle InvalidNetObjectFilterHandle = FNetObjectFilterHandle(0);
constexpr FNetObjectFilterHandle ToOwnerFilterHandle = FNetObjectFilterHandle(1);
/** ConnectionFilterHandle is for internal use only. */
constexpr FNetObjectFilterHandle ConnectionFilterHandle = FNetObjectFilterHandle(2);

/** Invalid group handle */
constexpr FNetObjectGroupHandle InvalidNetObjectGroupHandle = FNetObjectGroupHandle(0);

/** Special group, NetHandles assigned to this group will be filtered out for all connections */
constexpr FNetObjectGroupHandle NotReplicatedNetObjectGroupHandle = FNetObjectGroupHandle(1);

/** Special group, SubObjects assigned to this group will replicate to owner of RootParent */
constexpr FNetObjectGroupHandle NetGroupOwnerNetObjectGroupHandle = FNetObjectGroupHandle(2);

/** Special group, NetHandles assigned to this group will Replicate if replay netconditions is met  */
constexpr FNetObjectGroupHandle NetGroupReplayNetObjectGroupHandle = FNetObjectGroupHandle(3);

/** Returns true of the provided GroupHandle is a reserved NetObjectGroupHandle */
static constexpr bool IsReservedNetObjectGroupHandle(FNetObjectGroupHandle GroupHandle) { return GroupHandle >= NotReplicatedNetObjectGroupHandle && GroupHandle <= NetGroupReplayNetObjectGroupHandle; }

/** Used to control whether an object is allowed to be replicated or not. */
enum class ENetFilterStatus : uint32
{
	/** Do not allow replication. */
	Disallow,
	/** Allow replication. */
	Allow,
};

}

/**
 * Parameters passed to UNetObjectFilter::Filter.
 */
struct FNetObjectFilteringParams
{
	FNetObjectFilteringParams(const UE::Net::FNetBitArrayView InFilteredObjects);

	/** The indices of the objects that have this filter set. The indices of set bits correspond to the object indices. */
	const UE::Net::FNetBitArrayView FilteredObjects;

	/**
	 * The contents of OutAllowedObjects is undefined when passed to Filter(). The filter is responsible
	 * for setting and clearing bits for objects that have this filter set, which is provided in the
	 * FilteredObjects member. It's safe to set or clear all bits in the bitarray as the callee will
	 * only care about bits which the filter is responsible for.
	 */
	UE::Net::FNetBitArrayView OutAllowedObjects;

	/** FilteringInfos for all objects. Index using the set bit indices in FilteredObjects. */
	const FNetObjectFilteringInfo* FilteringInfos;

	/** State buffers for all objects. Index using the set bit indices in FilteredObjects. */
	uint8 *const* StateBuffers;

	/** ID of the connection that the filtering applies to. */
	uint32 ConnectionId;

	/** The view associated with the connection and its sub-connections that objects are filtered for. */
	UE::Net::FReplicationView View;
};

/**
 * Parameters passed to UNetObjectFilter::PreFilter.
 */
struct FNetObjectPreFilteringParams
{
};

/**
 * Parameters passed to UNetObjectFilter::PostFilter.
 */
struct FNetObjectPostFilteringParams
{
};

/**
 * Filter specific data stored per object, such as offsets to tags.
 * The data is initialized to zero by default.
 */
struct alignas(8) FNetObjectFilteringInfo
{
	uint16 Data[4];
};

/**
 * Base class for filter specific configuration.
 * @see FNetObjectFilterDefinition
 */
UCLASS(Transient, MinimalAPI)
class UNetObjectFilterConfig : public UObject
{
	GENERATED_BODY()
};

/** Parameters passed to the filter's Init() call. */
struct FNetObjectFilterInitParams
{
	TObjectPtr<const UReplicationSystem> ReplicationSystem;
	/** Optional config as set in the FNetObjectFilterDefinition. */
	UNetObjectFilterConfig* Config = nullptr;
	/** The maximum number of objects in the system. */
	uint32 MaxObjectCount = 0;
	/** The maximum number of connections in the system. */
	uint32 MaxConnectionCount = 0;
};

struct FNetObjectFilterAddObjectParams
{
	/** The info is zeroed before the AddObject() call. Fill in with filter specifics, like offsets to tags. */
	FNetObjectFilteringInfo& OutInfo;

	/** The FReplicationInstanceProtocol which describes the source state data. */
	const UE::Net::FReplicationInstanceProtocol* InstanceProtocol;

	/** The FReplicationProtocol which describes the internal state data. */
	const UE::Net::FReplicationProtocol* Protocol;

	/**
	 * One can retrieve relevant information from the object state buffer using the FReplicationProtocol.
	 * Note that this is the internal network representation of the data which is stored in quantized form.
	 * NetSerializers can dequantize the data to the original source data form.
	 */
	const uint8* StateBuffer;
};

/** Parameters passed to the filter's UpdateObjects() call. */
struct FNetObjectFilterUpdateParams
{
	/** Indices of the updated objects. */
	const uint32* ObjectIndices;
	/** The number of objects that have been updated. */
	uint32 ObjectCount;

	/** InstanceProtocols for updated objects. Index using 0..ObjectCount-1. */
	UE::Net::FReplicationInstanceProtocol const*const* InstanceProtocols;

	/** State buffers for all objects. Index using ObjectIndices[0..ObjectCount-1]. */
	uint8 *const* StateBuffers;

	/** Infos for all objects. Index using ObjectIndices[0..ObjectCount-1]. */
	FNetObjectFilteringInfo* FilteringInfos;
};

UCLASS()
class UNetObjectFilter : public UObject
{
	GENERATED_BODY()

public:
	/** Called right after constructor for enabled filters. Must be overriden. */ 
	IRISCORE_API virtual void Init(FNetObjectFilterInitParams&) PURE_VIRTUAL(Init,);

	/** A new connection has been added. An opportunity for the filter to allocate per connection info. */
	IRISCORE_API virtual void AddConnection(uint32 ConnectionId);

	/** A new connection has been added. An opportunity for the filter to deallocate per connection info. */
	IRISCORE_API virtual void RemoveConnection(uint32 ConnectionId);

	/** A new object want to use this filter. Opportunity to cache some information for it. The info struct passed has been zeroed. Must be overriden. */
	IRISCORE_API virtual bool AddObject(uint32 ObjectIndex, FNetObjectFilterAddObjectParams&) PURE_VIRTUAL(AddObject, return false;)

	/** An object no longer wants to use this filter. */
	IRISCORE_API virtual void RemoveObject(uint32 ObjectIndex, const FNetObjectFilteringInfo&) PURE_VIRTUAL(RemoveObject,)

	/** A set of objects using this filter has been updated. An opportunity for the filter to update cached data. */
	IRISCORE_API virtual void UpdateObjects(FNetObjectFilterUpdateParams&) PURE_VIRTUAL(UpdateObjects,)

	/**
	 * If there are any connections being replicated and there's a chance Filter() will be called then PreFilter()
	 * will be called exactly once before all calls to Filter().
	 */
	IRISCORE_API virtual void PreFilter(FNetObjectPreFilteringParams&);

	/** Filter a batch of objects. There may be multiple calls to this function even for the same connection. Must be overriden. */
	IRISCORE_API virtual void Filter(FNetObjectFilteringParams&);

	/**
	 * If PreFilter() was called then PostFilter() will be called exactly once after all Filter() calls.
	 */
	IRISCORE_API virtual void PostFilter(FNetObjectPostFilteringParams&);

protected:
	IRISCORE_API UNetObjectFilter();
};
