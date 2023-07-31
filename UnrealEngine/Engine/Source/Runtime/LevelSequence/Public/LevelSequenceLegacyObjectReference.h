// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "UObject/LazyObjectPtr.h"
#include "LevelSequenceLegacyObjectReference.generated.h"

/**
 * Legacy method by which objects were referenced within a level sequence. No longer used. See FLevelSequenceBindingReference for up-to-date implementation.
 */
USTRUCT()
struct FLevelSequenceLegacyObjectReference
{
	GENERATED_BODY();

	/**
	 * Resolve this reference within the specified context
	 *
	 * @return The object (usually an Actor or an ActorComponent).
	 */
	UObject* Resolve(UObject* InContext) const;

	/**
	 * Serialization operator
	 */
	friend FArchive& operator<<(FArchive& Ar, FLevelSequenceLegacyObjectReference& Ref)
	{
		Ar << Ref.ObjectId;
		Ar << Ref.ObjectPath;
		return Ar;
	}
	
	friend bool operator==(const FLevelSequenceLegacyObjectReference& A, const FLevelSequenceLegacyObjectReference& B)
	{
		if (A.ObjectId.IsValid() && A.ObjectId == B.ObjectId)
		{
			return true;
		}
		return A.ObjectPath == B.ObjectPath;
	}

private:

	/** Primary method of resolution - object ID, stored as an annotation on the object in the world, resolvable through TLazyObjectPtr */
	FUniqueObjectGuid ObjectId;

	/** Secondary method of resolution - path to the object within the context */
	FString ObjectPath;
};

USTRUCT()
struct FLevelSequenceObjectReferenceMap
{
public:

	GENERATED_BODY();

	UObject* ResolveBinding(const FGuid& ObjectId, UObject* InContext) const;

	/**
	 * Equality comparator required for proper UObject serialization (so we can compare against defaults)
	 */
	friend bool operator==(const FLevelSequenceObjectReferenceMap& A, const FLevelSequenceObjectReferenceMap& B)
	{
		return A.Map.OrderIndependentCompareEqual(B.Map);
	}
	
	/**
	 * Serialization function
	 */
	bool Serialize(FArchive& Ar);

	typedef TMap<FGuid, FLevelSequenceLegacyObjectReference> MapType;
	FORCEINLINE MapType::TRangedForIterator      begin()       { return Map.begin(); }
	FORCEINLINE MapType::TRangedForConstIterator begin() const { return Map.begin(); }
	FORCEINLINE MapType::TRangedForIterator      end  ()       { return Map.end();   }
	FORCEINLINE MapType::TRangedForConstIterator end  () const { return Map.end();   }

	TMap<FGuid, FLevelSequenceLegacyObjectReference> Map;
};

template<> struct TStructOpsTypeTraits<FLevelSequenceObjectReferenceMap> : public TStructOpsTypeTraitsBase2<FLevelSequenceObjectReferenceMap>
{
	enum { WithSerializer = true, WithIdenticalViaEquality = true };
};