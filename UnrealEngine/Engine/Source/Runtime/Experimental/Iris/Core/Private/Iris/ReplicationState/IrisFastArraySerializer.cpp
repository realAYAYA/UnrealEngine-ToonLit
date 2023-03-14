// Copyright Epic Games, Inc. All Rights Reserved.
#include "Iris/ReplicationState/IrisFastArraySerializer.h"
#include "Iris/ReplicationState/Private/IrisFastArraySerializerInternal.h"

FIrisFastArraySerializer::FIrisFastArraySerializer()
: FFastArraySerializer()
{
	InitChangeMask();
}

FIrisFastArraySerializer::FIrisFastArraySerializer(const FIrisFastArraySerializer& Other)
: FFastArraySerializer(Other)
{
	InitChangeMask();
}

FIrisFastArraySerializer::FIrisFastArraySerializer(const FIrisFastArraySerializer&& Other)
: FFastArraySerializer(Other)
{
	InitChangeMask();
}

FIrisFastArraySerializer& FIrisFastArraySerializer::operator=(const FIrisFastArraySerializer& Other)
{
	if (this != &Other)
	{
		Super::operator=(Other);
		// Currently we are pessimistic and marks everything as dirty
		if (ReplicationStateHeader.IsBound())
		{
			InternalMarkAllItemsChanged();
		}
	}

	return *this;
}

FIrisFastArraySerializer& FIrisFastArraySerializer::operator=(FIrisFastArraySerializer&& Other)
{
	if (this != &Other)
	{
		Super::operator=(Other);
		// Currently we are pessimistic and marks everything as dirty
		if (ReplicationStateHeader.IsBound())
		{
			InternalMarkAllItemsChanged();
		}
	}
	return *this;
}

void FIrisFastArraySerializer::InitChangeMask()
{
	// Init changemask and conditional changemask
	ChangeMaskStorage[0] = 0U;
	ChangeMaskStorage[1] = 0xffffffffU;
}

void FIrisFastArraySerializer::InternalMarkAllItemsChanged()
{
	checkSlow(ReplicationStateHeader.IsBound());
	UE::Net::Private::FIrisFastArraySerializerPrivateAccessor::MarkAllArrayItemsDirty(*this);
}

void FIrisFastArraySerializer::InternalMarkArrayAsDirty()
{
	checkSlow(ReplicationStateHeader.IsBound());
	UE::Net::Private::FIrisFastArraySerializerPrivateAccessor::MarkArrayDirty(*this);
}

void FIrisFastArraySerializer::InternalMarkItemChanged(int32 ItemIdx)
{
	checkSlow(ReplicationStateHeader.IsBound());
	UE::Net::Private::FIrisFastArraySerializerPrivateAccessor::MarkArrayItemDirty(*this, ItemIdx);
}