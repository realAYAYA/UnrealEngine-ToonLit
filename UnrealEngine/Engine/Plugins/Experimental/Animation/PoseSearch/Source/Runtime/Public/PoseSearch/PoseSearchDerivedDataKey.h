// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Queue.h"
#include "Containers/Set.h"
#include "Containers/UnrealString.h"
#include "Serialization/ArchiveUObject.h"
#include "Hash/Blake3.h"
#include "UObject/UnrealType.h"
#include "UObject/Object.h"

#if WITH_EDITORONLY_DATA

namespace UE::PoseSearch
{

//////////////////////////////////////////////////////////////////////////
// TObjectHasher
template <typename HashBuilder, typename HashDigest>
class POSESEARCH_API TObjectHasher : public FArchiveUObject
{
public:
	using HashDigestType = HashDigest;
	using HashBuilderType = HashBuilder;

	inline static const FName ExcludeFromHashName = FName(TEXT("ExcludeFromHash"));

	/**
	* Default constructor.
	*/
	TObjectHasher()
	{
		ArIgnoreOuterRef = true;

		// Set TObjectHasher to be a saving archive instead of a reference collector.
		// Reference collection causes FSoftObjectPtrs to be serialized by their weak pointer,
		// which doesn't give a stable hash.  Serializing these to a saving archive will
		// use a string reference instead, which is a more meaningful hash value.
		SetIsSaving(true);
	}

	//~ Begin FArchive Interface
	virtual bool ShouldSkipProperty(const FProperty* InProperty) const override
	{
		if (InProperty == nullptr)
		{
			return false;
		}

		if (FArchiveUObject::ShouldSkipProperty(InProperty))
		{
			return true;
		}

		if (InProperty->HasAllPropertyFlags(CPF_Transient))
		{
			return true;
		}
		const bool bExclude = InProperty->HasMetaData(ExcludeFromHashName);
		return bExclude;
	}

	virtual void Serialize(void* Data, int64 Length) override
	{
		Hasher.Update(reinterpret_cast<uint8*>(Data), Length);
	}

	virtual FArchive& operator<<(FName& Name) override
	{
		// Don't include the name of the object being serialized, since that isn't technically part of the object's state
		if (!ObjectBeingSerialized || (Name != ObjectBeingSerialized->GetFName()))
		{
			uint32 NameHash = GetTypeHash(Name);
			Hasher.Update(&NameHash, sizeof(NameHash));
		}

		return *this;
	}

	virtual FArchive& operator<<(class UObject*& Object) override
	{
		FArchive& Ar = *this;

		if (!RootObject || !Object || !Object->IsIn(RootObject))
		{
			auto UniqueName = GetPathNameSafe(Object);
			Ar << UniqueName;
		}
		else
		{
			ObjectsToSerialize.Enqueue(Object);
		}

		return Ar;
	}

	using FArchiveUObject::operator<<; // For visibility of the overloads we don't override

	virtual FString GetArchiveName() const override
	{
		return TEXT("TObjectHasher");
	}
	//~ End FArchive Interface

	/**
	* Serialize the given object and update the hash
	*/
	void Update(const UObject* Object, const UObject* Root)
	{
		RootObject = Root;
		if (Object)
		{
			// Start with the given object
			ObjectsToSerialize.Enqueue(Object);

			// Continue until we no longer have any objects to serialize
			while (ObjectsToSerialize.Dequeue(Object))
			{
				bool bAlreadyProcessed = false;
				ObjectsAlreadySerialized.Add(Object, &bAlreadyProcessed);
				// If we haven't already serialized this object
				if (!bAlreadyProcessed)
				{
					// Serialize it
					ObjectBeingSerialized = Object;
					if (!CustomSerialize(Object))
					{
						const_cast<UObject*>(Object)->Serialize(*this);
					}
					ObjectBeingSerialized = nullptr;
				}
			}

			// Cleanup
			RootObject = nullptr;
		}
	}

	void Update(const UObject* Object)
	{
		Update(Object, Object);
	}

	HashDigest Finalize()
	{
		return Hasher.Finalize();
	}

protected:
	virtual bool CustomSerialize(const UObject* Object)
	{
		return false;
	}

	HashBuilder Hasher;

	/** Queue of object references awaiting serialization */
	TQueue<const UObject*> ObjectsToSerialize;

	/** Set of objects that have already been serialized */
	TSet<const UObject*> ObjectsAlreadySerialized;

	/** Object currently being serialized */
	const UObject* ObjectBeingSerialized = nullptr;

	/** Root of object currently being serialized */
	const UObject* RootObject = nullptr;
};

using FObjectHasherBlake3 = TObjectHasher<FBlake3, FBlake3Hash>;

using FDerivedDataKeyBuilder = FObjectHasherBlake3;

} // namespace UE::PoseSearch

#endif // WITH_EDITORONLY_DATA