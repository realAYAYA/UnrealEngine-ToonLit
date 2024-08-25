// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Net/Core/NetBitArray.h"
#include "Iris/Core/NetChunkedArray.h"
#include "Iris/ReplicationSystem/ReplicationView.h"
#include "UObject/ObjectMacros.h"
#include "NetObjectFilter.generated.h"

struct FNetObjectFilteringInfo;
class UReplicationSystem;
namespace UE::Net
{
	typedef uint32 FNetObjectFilterHandle;
	struct FReplicationInstanceProtocol;
	struct FReplicationProtocol;
}

namespace UE::Net
{

constexpr FNetObjectFilterHandle InvalidNetObjectFilterHandle = FNetObjectFilterHandle(0);
constexpr FNetObjectFilterHandle ToOwnerFilterHandle = FNetObjectFilterHandle(1);
/** ConnectionFilterHandle is for internal use only. */
constexpr FNetObjectFilterHandle ConnectionFilterHandle = FNetObjectFilterHandle(2);

/** Used to control whether an object is allowed to be replicated or not. */
enum class ENetFilterStatus : uint32
{
	/** Do not allow replication. */
	Disallow,
	/** Allow replication. */
	Allow,
};

} // end namespace UE::Net

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
	const FNetObjectFilteringInfo* FilteringInfos = nullptr;

	/** State buffers for all objects. Index using the set bit indices in FilteredObjects. */
	const UE::Net::TNetChunkedArray<uint8*>* StateBuffers = nullptr;

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
	FNetObjectPreFilteringParams(const UE::Net::FNetBitArrayView InFilteredObjects);

	// The IDs of all valid connections.
	UE::Net::FNetBitArrayView ValidConnections;

	/** The indices of the objects that have this filter set. The indices of set bits correspond to the object indices. */
	const UE::Net::FNetBitArrayView FilteredObjects;

	/** FilteringInfos for all objects. Index using the set bit indices in FilteredObjects. */
	TArrayView<const FNetObjectFilteringInfo> FilteringInfos;
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

/** This configures when a filter gets executed inside PreSendUpdate and what data it has access to. */
UENUM()
enum class ENetFilterType : uint8
{
	/**
	 * The default setting of filters.
	 * This type of filter is applied before we poll objects and copy their data into network fragments.
	 * Thus a filter cannot operate on fragment data during it's operation, only whatever it stored or is accessing directly from the object.
	 * The benefit is that objects not relevant to any connection will get culled from being polled and forced to copy their dirty state for nothing.
	 */
	PrePoll_Raw,

	/**
	 * When set to FragmentBased, the filter gets access to the up to date fragment data of an object when it executes.
	 * Those objects will always be polled and have their dirty state copied even if not relevant.
	 */
	PostPoll_FragmentBased,
};

enum class ENetFilterTraits : uint8
{
	None = 0,
	Spatial = 1,
};
ENUM_CLASS_FLAGS(ENetFilterTraits);

/**
 * Base class for filter specific configuration.
 * @see FNetObjectFilterDefinition
 */
UCLASS(Transient, MinimalAPI, config=Engine)
class UNetObjectFilterConfig : public UObject
{
	GENERATED_BODY()

public:

	/** Can be used to modify when the filter is executed */
	UPROPERTY(Config)
	ENetFilterType FilterType = ENetFilterType::PrePoll_Raw;
};

/** Parameters passed to the filter's Init() call. */
struct FNetObjectFilterInitParams
{
public:
	FNetObjectFilterInitParams(const UE::Net::FNetBitArrayView InFilteredObjects)
	: FilteredObjects(InFilteredObjects)
	{
	}

	TObjectPtr<UReplicationSystem> ReplicationSystem;
	/** Optional config as set in the FNetObjectFilterDefinition. */
	UNetObjectFilterConfig* Config = nullptr;
	/** The maximum number of objects in the system. */
	uint32 MaxObjectCount = 0;
	/** The maximum number of connections in the system. */
	uint32 MaxConnectionCount = 0;
	/** The objects that are handled by the filter. */
	const UE::Net::FNetBitArrayView FilteredObjects;
};

struct FNetObjectFilterAddObjectParams
{
	/** The info is zeroed before the AddObject() call. Fill in with filter specifics, like offsets to tags. */
	FNetObjectFilteringInfo& OutInfo;

	/** Name of a specialized configuration profile. When none, the default settings are expected. */
	FName ProfileName;

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
	const uint32* ObjectIndices = nullptr;
	/** The number of objects that have been updated. */
	uint32 ObjectCount = 0;

	/** Infos for all objects. Index using ObjectIndices[0..ObjectCount-1]. */
	FNetObjectFilteringInfo* FilteringInfos = nullptr;

	/** 
	* InstanceProtocols for updated objects. Index using 0..ObjectCount-1.
	* NOTE: Only for filters of type FragmentBased; null for Raw types
	*/
	UE::Net::FReplicationInstanceProtocol const* const* InstanceProtocols = nullptr;

	/**
	* State buffers for all objects. Index using ObjectIndices[0..ObjectCount-1].
	* NOTE: Only for filters of type FragmentBased; null for Raw types
	*/
	const UE::Net::TNetChunkedArray<uint8*>* StateBuffers = nullptr;
};

UCLASS(Abstract)
class UNetObjectFilter : public UObject
{
	GENERATED_BODY()

public:
	IRISCORE_API void Init(FNetObjectFilterInitParams& Params);

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

	/** Returns what type of filter it is. Default is to cull and be executed before dirty state copying. */
	ENetFilterType GetFilterType() const { return FilterType; }

	/** Returns the filter's traits. */
	ENetFilterTraits GetFilterTraits() const { return FilterTraits; }

	struct FDebugInfoParams
	{
		FName FilterName;

		const FNetObjectFilteringInfo* FilteringInfos = nullptr;

		/** ID of the connection that the filtering applies to. */
		uint32 ConnectionId = 0;

		/** The view associated with the connection and its sub-connections that objects are filtered for. */
		UE::Net::FReplicationView View;
	};

	IRISCORE_API virtual FString PrintDebugInfoForObject(const FDebugInfoParams& Params, uint32 ObjectIndex) const { return Params.FilterName.ToString(); };

protected:
	IRISCORE_API UNetObjectFilter();

	/** Called right after constructor for enabled filters. Must be overriden. */
	IRISCORE_API virtual void OnInit(FNetObjectFilterInitParams&) PURE_VIRTUAL(OnInit, );

	/* Returns true if the object is added to this filter, false otherwise. */
	bool IsAddedToFilter(uint32 ObjectIndex) const;

	/* Returns the filtering info for this object if it's handled by this filter, nullptr otherwise. */
	FNetObjectFilteringInfo* GetFilteringInfo(uint32 ObjectIndex);

	/** Directly set when you want your dynamic filter to be executed. */
	void SetupFilterType(ENetFilterType NewFilterType) { FilterType = NewFilterType; }

	/** Adds traits. */
	void AddFilterTraits(ENetFilterTraits Traits);

	/** Sets the traits specified by TraitsMask to Traits. */
	void SetFilterTraits(ENetFilterTraits Traits, ENetFilterTraits TraitsMask);

private:
	class FFilterInfo
	{
	public:
		FFilterInfo() = default;
		FFilterInfo(const UE::Net::FNetBitArrayView FilteredObjects, const TArrayView<FNetObjectFilteringInfo> FileringInfos);

		FFilterInfo& operator=(const FFilterInfo&);

		bool IsAddedToFilter(uint32 ObjectIndex) const;

		FNetObjectFilteringInfo* GetFilteringInfo(uint32 ObjectIndex);

	private:
		const UE::Net::FNetBitArrayView FilteredObjects;
		TArrayView<FNetObjectFilteringInfo> FilteringInfos;
	};

	ENetFilterType FilterType = ENetFilterType::PrePoll_Raw;
	ENetFilterTraits FilterTraits = ENetFilterTraits::None;
	FFilterInfo FilterInfo;
};

inline bool UNetObjectFilter::IsAddedToFilter(uint32 ObjectIndex) const
{
	return FilterInfo.IsAddedToFilter(ObjectIndex);
}

inline FNetObjectFilteringInfo* UNetObjectFilter::GetFilteringInfo(uint32 ObjectIndex)
{
	return FilterInfo.GetFilteringInfo(ObjectIndex);
}

inline bool UNetObjectFilter::FFilterInfo::IsAddedToFilter(uint32 ObjectIndex) const
{
	return ObjectIndex < FilteredObjects.GetNumBits() && FilteredObjects.IsBitSet(ObjectIndex);
}

inline void UNetObjectFilter::AddFilterTraits(ENetFilterTraits Traits)
{
	FilterTraits |= Traits;
}

inline void UNetObjectFilter::SetFilterTraits(ENetFilterTraits Traits, ENetFilterTraits TraitsMask)
{
	const ENetFilterTraits NewFilterTraits = (FilterTraits & ~TraitsMask) | (Traits & TraitsMask);
	FilterTraits = NewFilterTraits;
}
