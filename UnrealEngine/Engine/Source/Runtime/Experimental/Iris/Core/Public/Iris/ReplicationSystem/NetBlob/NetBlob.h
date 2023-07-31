// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreTypes.h"
#include "Iris/Core/NetObjectReference.h"
#include "Iris/ReplicationState/ReplicationStateDescriptor.h"
#include "Containers/ArrayView.h"
#include "Templates/RefCounting.h"
#include "Templates/UniquePtr.h"
#include <atomic>

namespace UE::Net
{
	class FNetBlob;
	class FNetSerializationContext;
	struct FReplicationStateDescriptor;
	namespace Private
	{
		class FNetBlobManager;
	}
}

namespace UE::Net
{

/** Flags that can be set at blob creation time. */
enum class ENetBlobFlags : uint32
{
	None = 0,

	/** The blob should be delivered reliably in order with respect to other reliable blobs. */
	Reliable = 1U << 0U,

	/** Used for FRawDataNetBlob derived classes to avoid duplicate serialization when splitting large blob. */
	RawDataNetBlob = Reliable << 1U,

	/** Used to indicate that this blob have ObjectReferences that might have to be exported. */
	HasExports = RawDataNetBlob << 1U,
};
ENUM_CLASS_FLAGS(ENetBlobFlags);

typedef uint32 FNetBlobType;

/** Denotes an invalid NetBlob type. */
constexpr FNetBlobType InvalidNetBlobType = ~FNetBlobType(0);

/**
 * Information typically passed to a FNetBlob at creation time.
 * Flags will not be serialized unless the FNetBlob derived class chooses to do so.
 */
struct FNetBlobCreationInfo
{
	FNetBlobType Type = InvalidNetBlobType;
	ENetBlobFlags Flags = ENetBlobFlags::None;
};

class FNetBlob
{
public:
	/** Construct a NetBlob with reference count zero. */
	IRISCORE_API FNetBlob(const FNetBlobCreationInfo&);

	/** Set the blob state. Use when there's a descriptor to avoid having to override serialization functions. */
	IRISCORE_API void SetState(const TRefCountPtr<const FReplicationStateDescriptor>& BlobDescriptor, TUniquePtr<uint8> QuantizedBlobState);

	/** Returns the FNetBlobCreationInfo. */
	const FNetBlobCreationInfo& GetCreationInfo() const { return CreationInfo; }

	/** Returns the FReplicationStateDescriptor if there is one. It's recommended to use a descriptor instead of overriding the serialization methods. */
	const FReplicationStateDescriptor* GetReplicationStateDescriptor() const { return BlobDescriptor.GetReference(); }

	/** Serializes the necessary parts of the creation info so that the blob can be recreated on the receiving side. */
	IRISCORE_API static void SerializeCreationInfo(FNetSerializationContext& Context, const FNetBlobCreationInfo& CreationInfo);

	/** Deserializes the necessary parts of the creation info so that the correct blob can be created. */
	IRISCORE_API static void DeserializeCreationInfo(FNetSerializationContext& Context, FNetBlobCreationInfo& OutCreationInfo);

	/** Serialize the blob together with/targeting a specific object knowing the NetHandle has already been serialized. */
	IRISCORE_API virtual void SerializeWithObject(FNetSerializationContext& Context, FNetHandle NetHandle) const;

	/** Deserialize a blob that was serialized with SerializeWithObject. */
	IRISCORE_API virtual void DeserializeWithObject(FNetSerializationContext& Context, FNetHandle NetHandle);

	/** Serialize the blob. */
	IRISCORE_API virtual void Serialize(FNetSerializationContext& Context) const;

	/** Deserialize a blob that was serialized with Serialize. */
	IRISCORE_API virtual void Deserialize(FNetSerializationContext& Context);

	/**
	 * Returns the status of the object reference resolving.
	 * @see ENetObjectReferenceResolveResult
	 */
	IRISCORE_API ENetObjectReferenceResolveResult ResolveObjectReferences(FNetSerializationContext& Context) const;

	/** Adds a reference. A blob is created with reference count zero. */
	void AddRef() const { ++RefCount; }

	/** Removes a reference. When the reference count reaches zero the blob will be deleted. A blob is created with reference count zero. */
	IRISCORE_API void Release() const;

	/** Returns the reference count.  A blob is created with reference count zero. */
	int32 GetRefCount() const { return RefCount; }

	/** Retrieve the object references that need to be exported.  */
	TArrayView<const FNetObjectReference> CallGetExports() const { return EnumHasAnyFlags(CreationInfo.Flags, ENetBlobFlags::HasExports) ? GetExports() : MakeArrayView<const FNetObjectReference>(nullptr, 0); };

protected:
	FNetBlob(const FNetBlob&) = delete;
	FNetBlob& operator=(const FNetBlob&) = delete;

	/** The destructor will free dynamic state, if present, in the state buffer and remove a reference to the descriptor. */
	IRISCORE_API virtual ~FNetBlob();

	/** Override to return the object references that need to be exported. */
	virtual TArrayView<const FNetObjectReference> GetExports() const;

	/** Serializes the state if there's a valid descriptor and state buffer. */
	IRISCORE_API void SerializeBlob(FNetSerializationContext& Context) const;

	/** Deserializes the state if there's a valid descriptor and state buffer. */
	IRISCORE_API void DeserializeBlob(FNetSerializationContext& Context);

protected:
	/** The CreationInfo that was passed to the constructor. */
	FNetBlobCreationInfo CreationInfo;

	/** The state descriptor. */
	TRefCountPtr<const FReplicationStateDescriptor> BlobDescriptor;

	/** The state buffer that holds the data described by the descriptor. */
	TUniquePtr<uint8> QuantizedBlobState;

private:
	mutable std::atomic<int32> RefCount;
};

/**
 * FNetObjectAttachment serves as a base class for NetBlobs targeting a specific object.
 * @see UReplicationSystem::QueueNetObjectAttachment
 */
class FNetObjectAttachment : public FNetBlob
{
public:
	IRISCORE_API FNetObjectAttachment(const FNetBlobCreationInfo&);
	const FNetObjectReference& GetNetObjectReference() const { return NetObjectReference; }

protected:
	virtual ~FNetObjectAttachment();

	/** Serializes the owner and subobject reference. */
	IRISCORE_API void SerializeObjectReference(FNetSerializationContext& Context) const;

	/** Serializes the owner and subobject reference. */
	IRISCORE_API void DeserializeObjectReference(FNetSerializationContext& Context);

	/** Serializes only the subobject reference and assumes the passed NetHandle is the owner. */
	IRISCORE_API void SerializeSubObjectReference(FNetSerializationContext& Context, FNetHandle NetHandle) const;

	/** Deserializes a subobject reference that was serialized using SerializeSubObjectReference with the same NetHandle. */
	IRISCORE_API void DeserializeSubObjectReference(FNetSerializationContext& Context, FNetHandle NetHandle);

	/** Set the owner and subobject references. */
	void SetNetObjectReference(const FNetObjectReference& InQueueOwnerReference, const FNetObjectReference& InTargetObjectReference);

protected:
	friend class Private::FNetBlobManager;

	/** The owner reference. */
	FNetObjectReference NetObjectReference;

	/** The subobject reference. */
	FNetObjectReference TargetObjectReference;
};

inline void FNetObjectAttachment::SetNetObjectReference(const FNetObjectReference& InQueueOwnerReference, const FNetObjectReference& InTargetObjectReference)
{
	NetObjectReference = InQueueOwnerReference;
	if (InQueueOwnerReference != InTargetObjectReference)
	{
		TargetObjectReference = InTargetObjectReference;
	}
}

}
