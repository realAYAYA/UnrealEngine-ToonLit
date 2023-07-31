// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "HAL/Platform.h"
#include "UObject/CoreNetTypes.h"
#include "UObject/ObjectKey.h"

class UActorComponent;
class UObject;

namespace UE::Net
{
	class FSubObjectRegistryGetter;
}

namespace UE::Net 
{

/**
* Stores the SubObjects to replicate and the network condition dictating to which connection they can replicate to.
*/
struct NETCORE_API FSubObjectRegistry
{
public:

	enum class EResult : uint8
	{
		/** SubObject is new in the registry */
		NewEntry,
		/** SubObject already was in the registry */
		AlreadyRegistered,
		/** SubObject was already registered but the NetCondition passed is different from the entry */
		NetConditionConflict,
	};

	/** Adds a subobject and returns if it's a new entry or existing entry */
	FSubObjectRegistry::EResult AddSubObjectUnique(UObject* InSubObject, ELifetimeCondition InNetCondition);

	/** Remove the subobject from the replicated list. Returns true if the subobject had been registered. */
	bool RemoveSubObject(UObject* InSubObject);

	/** Find the NetCondition of a SubObject. Returns COND_MAX if not registered */
	ELifetimeCondition GetNetCondition(UObject* SubObject) const;

	struct FEntry
	{
		/** Raw pointer since users are obligated to call RemoveReplicatedSubobject before destroying the subobject otherwise it will cause a crash.  */
		UObject* SubObject = nullptr;

		/** Store the object key info for fast access later */
		FObjectKey Key;

		/** The network condition that chooses which connection this subobject can be replicated to. Default is none which means all connections receive it. */
		ELifetimeCondition NetCondition = COND_None;

		FEntry(UObject* InSubObject, ELifetimeCondition InNetCondition = COND_None)
			: SubObject(InSubObject)
			, Key(InSubObject)
			, NetCondition(InNetCondition)
		{
		}

		bool operator==(const FEntry& rhs) const { return Key == rhs.Key; }
		bool operator==(const UObject* rhs) const { return SubObject == rhs; }
	};

	/** Returns the list of registered subobjects */
	const TArray<FSubObjectRegistry::FEntry>& GetRegistryList() const { return Registry; }

	/** Returns true if the subobject is contained in this list */
	bool IsSubObjectInRegistry(const UObject* SubObject) const;

private:

	TArray<FEntry> Registry;
};

/** Keep track of replicated components and their subobject list */
struct FReplicatedComponentInfo
{
	/** Component that will be replicated */
	UActorComponent* Component = nullptr;

	/** Store the object key info for fast access later */
	FObjectKey Key;

	/** NetCondition of the component */
	ELifetimeCondition NetCondition = COND_None;

	/** Collection of subobjects replicated with this component */
	FSubObjectRegistry SubObjects;

	FReplicatedComponentInfo(UActorComponent* InComponent, ELifetimeCondition InNetCondition = COND_None)
		: Component(InComponent)
		, Key((UObject*)InComponent)
		, NetCondition(InNetCondition)
	{
	};

	bool operator==(const FReplicatedComponentInfo& rhs) const { return Key == rhs.Key; }
	bool operator==(const UActorComponent* rhs) const { return Component == rhs; }
};


} // namespace UE::Net


