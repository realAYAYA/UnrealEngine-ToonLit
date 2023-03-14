// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Net/Serialization/FastArraySerializer.h"
#include "Iris/ReplicationState/ReplicationStateFwd.h"
#include "IrisFastArraySerializer.generated.h"

// Forward  declarations
namespace UE::Net::Private
{

struct FIrisFastArraySerializerPrivateAccessor;

}

/**
 * Specialization of FFastArraySerializer in order to add state tracking support for Iris
 * Current usage is to inherit from this struct instead of FFastArraySerializer, backwards compatible with existing system as it simply forwards calls to MarkDirty/MarkItemDirty
 * This class could be named FFastArrayReplicationState, but kept the FIrisFastArraySerializer to match old naming for the time being
 */
USTRUCT()
struct FIrisFastArraySerializer : public FFastArraySerializer
{
	// At the moment as we have no way to specify this we reserve 31 bits that can be used, the first bit is used for the array itself
	enum { IrisFastArrayChangeMaskBits = 31U };
	enum { IrisFastArrayChangeMaskBitOffset = 1U };
	enum { IrisFastArrayPropertyBitIndex = 0U };	

	GENERATED_BODY()
	IRISCORE_API FIrisFastArraySerializer();

	~FIrisFastArraySerializer() = default;

	/** Will not copy replication state header */
	IRISCORE_API FIrisFastArraySerializer(const FIrisFastArraySerializer& Other);

	/** We must make sure that we do not copy replication state header and must update dirtiness if bound */
	IRISCORE_API FIrisFastArraySerializer& operator=(const FIrisFastArraySerializer& Other);

	/** Will not copy replication state header */
	IRISCORE_API FIrisFastArraySerializer(const FIrisFastArraySerializer&& Other);

	/** We must make sure that we do not move replication state header and must update dirtiness if bound */
	IRISCORE_API FIrisFastArraySerializer& operator=(FIrisFastArraySerializer&& Other);
	
	/** Override MarkItemDirty in order to mark object as dirty in the DirtyNetObjectTracker */
	IRISCORE_API void MarkItemDirty(FFastArraySerializerItem & Item)
	{ 
		FFastArraySerializer::MarkItemDirty(Item);
		if (ReplicationStateHeader.IsBound())
		{
			// Mark array dirty for Iris so that is it copied
			InternalMarkArrayAsDirty();
		}
	}
	
	/** Override MarkArrayDirty in order to mark object as dirty in the DirtyNetObjectTracker */
	IRISCORE_API void MarkArrayDirty()
	{
		FFastArraySerializer::MarkArrayDirty();
		if (ReplicationStateHeader.IsBound())
		{
			// Mark array dirty for Iris
			InternalMarkArrayAsDirty();
		}	
	}

	friend UE::Net::Private::FIrisFastArraySerializerPrivateAccessor;

private:
	// Mark item at index as changed, and set the array as dirty
	void InitChangeMask();
	void InternalMarkItemChanged(int32 ItemIdx);
	void InternalMarkAllItemsChanged();
	void InternalMarkArrayAsDirty();

	// Storage for changemask, this is currently hardcoded
	UPROPERTY(Transient, NotReplicated)
	uint32 ChangeMaskStorage[2];

	// Header for dirty state tracking
	UE::Net::FReplicationStateHeader ReplicationStateHeader;
};

