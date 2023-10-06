// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Chaos/Core.h"
#include "ParticleHandleFwd.h"

class FGeometryCollectionPhysicsProxy;

namespace Chaos
{
	class FRigidClustering;
	class FStrainModifierAccessor;
	class FStrainedProxyModifier;
	class FStrainedProxyIterator;
	class FStrainedProxyRange;
	class FStrainModifierAccessor;

	enum EStrainTypes : uint8
	{
		InternalStrain = 1 << 0,
		ExternalStrain = 1 << 1,
		CollisionStrain = 1 << 2
	};

	// FStrainedProxyModifier
	//
	// User-facing api for accessing the proxy of a strained cluster. Provides const access to the
	// proxy and limited read/write access to its internal strain data.
	class FStrainedProxyModifier
	{
	public:
		FStrainedProxyModifier(FRigidClustering& InRigidClustering, FGeometryCollectionPhysicsProxy* InProxy)
			: RigidClustering(InRigidClustering)
			, Proxy(InProxy)
			, RootHandle(InitRootHandle(InProxy))
			, RestChildren(InitRestChildren(InProxy))
		{ }

		FStrainedProxyModifier(const FStrainedProxyModifier& Other)
			: RigidClustering(Other.RigidClustering)
			, Proxy(Other.Proxy)
			, RestChildren(Other.RestChildren)
		{ }

		// Get the proxy that owns the strained cluster or clusters
		CHAOS_API const FGeometryCollectionPhysicsProxy* GetProxy() const;

		// Get the physics handle for the strained parent cluster
		CHAOS_API const Chaos::FPBDRigidParticleHandle* GetRootHandle() const;

		// Get the number of level-1 strainable entities (number of rest-children in the per-particle
		// strain model, or number of rest-connections in the edge/area model).
		CHAOS_API int32 GetNumRestBreakables() const;

		// Get the number of breaking strains amongst the level-1 strainables
		// If DoubleCount is true, then add N for each strain which is strong enough to
		// break a connection N times.
		CHAOS_API int32 GetNumBreakingStrains(bool bDoubleCount = true, const uint8 StrainTypes = EStrainTypes::ExternalStrain | EStrainTypes::CollisionStrain) const;

		// Clear strains for all strained cluster children
		CHAOS_API void ClearStrains();

	private:

		static CHAOS_API Chaos::FPBDRigidClusteredParticleHandle* InitRootHandle(FGeometryCollectionPhysicsProxy* Proxy);

		static CHAOS_API const TSet<int32>* InitRestChildren(FGeometryCollectionPhysicsProxy* Proxy);

		FRigidClustering& RigidClustering;
		FGeometryCollectionPhysicsProxy* Proxy;
		Chaos::FPBDRigidClusteredParticleHandle* RootHandle;
		const TSet<int32>* RestChildren;
	};

	// FStrainedProxyIterator
	//
	// Iterator produced by FStrainedProxyRange, for looping over proxies which are
	// associated with rigid clusters which have been strained.
	class FStrainedProxyIterator
	{
	public:
		FStrainedProxyIterator(FRigidClustering& InRigidClustering, TArray<FGeometryCollectionPhysicsProxy*>& InProxies, int32 InIndex)
			: RigidClustering(InRigidClustering)
			, Proxies(InProxies)
			, Index(InIndex)
		{ }

		FStrainedProxyIterator(const FStrainedProxyIterator& Other)
			: RigidClustering(Other.RigidClustering)
			, Proxies(Other.Proxies)
			, Index(Other.Index)
		{ }

		CHAOS_API FStrainedProxyModifier operator*();
		CHAOS_API FStrainedProxyIterator& operator++();
		CHAOS_API bool operator==(const FStrainedProxyIterator& Other) const;
		bool operator!=(const FStrainedProxyIterator& Other) const
		{
			return !operator==(Other);
		}

	private:
		FRigidClustering& RigidClustering;
		TArray<FGeometryCollectionPhysicsProxy*>& Proxies;
		int32 Index;
	};

	// FStrainedProxyRange
	//
	// Provides an interface for use with ranged-for, for iterating over strained proxies.
	// Constructor produces filtered array of proxies, and begin and end functions produce
	// iterators which can modify strain related properties of the clusters associated
	// with each proxy.
	class FStrainedProxyRange
	{
	public:
		CHAOS_API FStrainedProxyRange(Chaos::FRigidClustering& InRigidClustering, bool bRootLevelOnly);

		FStrainedProxyRange(const FStrainedProxyRange& Other)
			: RigidClustering(Other.RigidClustering)
			, Proxies(Other.Proxies)
		{ }

		FStrainedProxyIterator begin()
		{
			return FStrainedProxyIterator(RigidClustering, Proxies, 0);
		}

		FStrainedProxyIterator end()
		{
			return FStrainedProxyIterator(RigidClustering, Proxies, Proxies.Num());
		}

	private:
		FRigidClustering& RigidClustering;
		TArray<FGeometryCollectionPhysicsProxy*> Proxies;
	};

	// FStrainModifierAccessor
	//
	// Provides access to strained proxies and clusters
	class FStrainModifierAccessor
	{
	public:

		FStrainModifierAccessor(FRigidClustering& InRigidClustering) : RigidClustering(InRigidClustering) { }

		// Get an iterable range of unique geometry collection proxies which
		// correspond to all strained clusters. Optionally, only include proxies
		// for whom the strained parent is still the original root (ie, unbroken).
		CHAOS_API FStrainedProxyRange GetStrainedProxies(bool bRootLevelOnly = true);

	private:

		FRigidClustering& RigidClustering;
	};
}
