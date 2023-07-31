// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/ArrayCollectionArray.h"
#include "Chaos/PBDRigidParticles.h"
#include "Chaos/ImplicitObjectUnion.h"

namespace Chaos
{
	class FImplicitObjectUnionClustered;

/** 
 * Used within the clustering system to describe the clustering hierarchy. The ClusterId
 * stores the children IDs, and a Parent ID. When Id == \c nullptr the cluster is not
 * controlled by another body. 
 *
 * TODO: Chaos - Add dimension template param?  Add floating point param?
 */
struct ClusterId
{
	ClusterId() 
		: Id(nullptr)
		, NumChildren(0) 
	{}
	ClusterId(FPBDRigidParticleHandle* NewId, int NumChildrenIn)
		: Id(NewId)
		, NumChildren(NumChildrenIn) 
	{}
	FPBDRigidParticleHandle* Id;
	int32 NumChildren;
};


/**
 * An entry in a clustered particle's \c ConnectivityEdges array, indicating a
 * connection between that body and \c Sibling, with a strength breakable by 
 * a \c Strain threshold.
 *
 * TODO: Chaos - Add dimension template param?
 */
template <typename T>
struct TConnectivityEdge
{
	TConnectivityEdge() 
		: Sibling(nullptr)
		, Strain(0.0)
	{}

	TConnectivityEdge(TPBDRigidParticleHandle<T, 3>* InSibling, const T InStrain)
		: Sibling(InSibling)
		, Strain(InStrain) 
	{}

	TConnectivityEdge(const TConnectivityEdge& Other)
		: Sibling(Other.Sibling)
		, Strain(Other.Strain) 
	{}

	/** Compares by \p OtherSibling only, for \c TArray::FindByKey(). */
	bool operator==(const TPBDRigidParticleHandle<T, 3>* OtherSibling) const
	{ return Sibling == OtherSibling; }

	TPBDRigidParticleHandle<T, 3>* Sibling;
	T Strain;
};
typedef TConnectivityEdge<FReal> FConnectivityEdge;
typedef TArray<FConnectivityEdge> FConnectivityEdgeArray;

template<class T, int d>
class TPBDRigidClusteredParticles : public TPBDRigidParticles<T, d>
{
  public:
	TPBDRigidClusteredParticles()
	: TPBDRigidParticles<T, d>()
	{
		InitHelper();
	}
	TPBDRigidClusteredParticles(const TPBDRigidParticles<T, d>& Other) = delete;
	TPBDRigidClusteredParticles(TPBDRigidParticles<T, d>&& Other)
	: TPBDRigidParticles<T, d>(MoveTemp(Other))
	, MClusterIds(MoveTemp(Other.MClusterIds))
	, MChildToParent(MoveTemp(Other.MChildToParent))
	, MClusterGroupIndex(MoveTemp(Other.MClusterGroupIndex))
	, MInternalCluster(MoveTemp(Other.MInternalCluster))
	, MChildrenSpatial(MoveTemp(Other.MChildrenSpatial))
	, MPhysicsProxies(MoveTemp(Other.MPhysicsProxies))
	, MCollisionImpulses(MoveTemp(Other.MCollisionImpulses))
	, MStrains(MoveTemp(Other.MStrains))
	, MConnectivityEdges(MoveTemp(Other.MConnectivityEdges))
	, MExternalStrains(MoveTemp(Other.MExternalStrains))
	, MAnchored(MoveTemp(Other.MAnchored))
	{
		InitHelper();
	}
	~TPBDRigidClusteredParticles() {}

	const auto& ClusterIds(int32 Idx) const { return MClusterIds[Idx]; }
	auto& ClusterIds(int32 Idx) { return MClusterIds[Idx]; }

	const auto& ChildToParent(int32 Idx) const { return MChildToParent[Idx]; }
	auto& ChildToParent(int32 Idx) { return MChildToParent[Idx]; }

	const auto& ClusterGroupIndex(int32 Idx) const { return MClusterGroupIndex[Idx]; }
	auto& ClusterGroupIndex(int32 Idx) { return MClusterGroupIndex[Idx]; }

	const auto& InternalCluster(int32 Idx) const { return MInternalCluster[Idx]; }
	auto& InternalCluster(int32 Idx) { return MInternalCluster[Idx]; }

	const auto& ChildrenSpatial(int32 Idx) const { return MChildrenSpatial[Idx]; }
	auto& ChildrenSpatial(int32 Idx) { return MChildrenSpatial[Idx]; }

	const auto& PhysicsProxies(int32 Idx) const { return MPhysicsProxies[Idx]; }
	auto& PhysicsProxies(int32 Idx) { return MPhysicsProxies[Idx]; }

	const auto& CollisionImpulses(int32 Idx) const { return MCollisionImpulses[Idx]; }
	auto& CollisionImpulses(int32 Idx) { return MCollisionImpulses[Idx]; }
	auto& CollisionImpulsesArray() { return MCollisionImpulses; }

	const auto& ExternalStrains(int32 Idx) const { return MExternalStrains[Idx]; }
	auto& ExternalStrains(int32 Idx) { return MExternalStrains[Idx]; }
	auto& ExternalStrainsArray() { return MExternalStrains; }
	
	const auto& Strains(int32 Idx) const { return MStrains[Idx]; }
	auto& Strains(int32 Idx) { return MStrains[Idx]; }

	const auto& ConnectivityEdges(int32 Idx) const { return MConnectivityEdges[Idx]; }
	auto& ConnectivityEdges(int32 Idx) { return MConnectivityEdges[Idx]; }

	const bool& Anchored(int32 Idx) const { return MAnchored[Idx]; }
	bool& Anchored(int32 Idx) { return MAnchored[Idx]; }
	
	const auto& ConnectivityEdgesArray() const { return MConnectivityEdges; }

	const auto& ClusterIdsArray() const { return MClusterIds; }
	auto& ClusterIdsArray() { return MClusterIds; }

	const auto& ChildToParentArray() const { return MChildToParent; }
	auto& ChildToParentArray() { return MChildToParent; }

	const auto& StrainsArray() const { return MStrains; }
	auto& StrainsArray() { return MStrains; }

	const auto& ClusterGroupIndexArray() const { return MClusterGroupIndex; }
	auto& ClusterGroupIndexArray() { return MClusterGroupIndex; }

	const auto& InternalClusterArray() const { return MInternalCluster; }
	auto& InternalClusterArray() { return MInternalCluster; }

	const auto& AnchoredArray() const { return MAnchored; }
	auto& AnchoredArray() { return MAnchored; }

	
	typedef TPBDRigidClusteredParticleHandle<T, d> THandleType;
	const THandleType* Handle(int32 Index) const { return static_cast<const THandleType*>(TGeometryParticles<T,d>::Handle(Index)); }

	//cannot be reference because double pointer would allow for badness, but still useful to have non const access to handle
	THandleType* Handle(int32 Index) { return static_cast<THandleType*>(TGeometryParticles<T, d>::Handle(Index)); }

	
  private:

	  void InitHelper()
	  {
		  this->MParticleType = EParticleType::Clustered;
		  TArrayCollection::AddArray(&MClusterIds);
		  TArrayCollection::AddArray(&MChildToParent);
		  TArrayCollection::AddArray(&MClusterGroupIndex);
		  TArrayCollection::AddArray(&MInternalCluster);
		  TArrayCollection::AddArray(&MChildrenSpatial);
		  TArrayCollection::AddArray(&MPhysicsProxies);
		  TArrayCollection::AddArray(&MCollisionImpulses);
		  TArrayCollection::AddArray(&MStrains);
		  TArrayCollection::AddArray(&MConnectivityEdges);
	  	  TArrayCollection::AddArray(&MExternalStrains);
	  	  TArrayCollection::AddArray(&MAnchored);
	  }

	  TArrayCollectionArray<ClusterId> MClusterIds;
	  TArrayCollectionArray<TRigidTransform<T, d>> MChildToParent;
	  TArrayCollectionArray<int32> MClusterGroupIndex;
	  TArrayCollectionArray<bool> MInternalCluster;
	  TArrayCollectionArray<TUniquePtr<FImplicitObjectUnionClustered>> MChildrenSpatial;

	  // Multiple proxy pointers required for internal clusters
	  TArrayCollectionArray<TSet<IPhysicsProxyBase*>> MPhysicsProxies;

	  // Collision Impulses
	  TArrayCollectionArray<T> MCollisionImpulses;

	  // external strains ( use by fields )
	  // @todo(chaos) we should eventually merge MCollisionImpulses into MExternalStrains when Clustering code has been updated to not clear the impulses just before processing them 
	  TArrayCollectionArray<T> MExternalStrains; 

	  // User set parameters
	  TArrayCollectionArray<T> MStrains;

	  TArrayCollectionArray<TArray<TConnectivityEdge<T>>> MConnectivityEdges;

	  // make the particle act as a kinematic anchor,
	  // this allows the particle to be broken off while still be anchor contributor through the connection graph   
	  TArrayCollectionArray<bool> MAnchored;
};

using FPBDRigidClusteredParticles = TPBDRigidClusteredParticles<FReal, 3>;

} // namespace Chaos
