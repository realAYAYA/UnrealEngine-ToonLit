// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDRigidClustering.h"

namespace Chaos
{
	enum CHAOS_API EMassOffsetType : uint8
	{
		EPosition = 1 << 0,
		ERotation = 1 << 1
	};
	ENUM_CLASS_FLAGS(EMassOffsetType)

	void CHAOS_API UpdateClusterMassProperties(
		FPBDRigidClusteredParticleHandle* Parent,
		const TSet<FPBDRigidParticleHandle*>& Children);

	// If bPosition == true, set X/P of Cluster to its world CoM, and set its CoM to ZeroVector
	// If bRotation == true, set R/Q of Cluster to its world RoM, and set its RoM to Identity
	void CHAOS_API MoveClusterToMassOffset(FPBDRigidClusteredParticleHandle* Cluster, EMassOffsetType MassOffsetTypes);

	void CHAOS_API UpdateKinematicProperties(
		FPBDRigidParticleHandle* Parent,
		const FRigidClustering::FClusterMap&,
		FRigidClustering::FRigidEvolution&);

	void CHAOS_API UpdateGeometry(
		Chaos::FPBDRigidClusteredParticleHandle* Parent,
		const TSet<FPBDRigidParticleHandle*>& Children,
		const FRigidClustering::FClusterMap& ChildrenMap,
		TSharedPtr<Chaos::FImplicitObject, ESPMode::ThreadSafe> ProxyGeometry,
		const FClusterCreationParameters& Parameters);


	/**
	* Update shapes with first valid filter data from a child. Should be called after a child's filter
	* is modified to ensure cluster's filters match at least one of the children and isn't stale.
	*/
	void UpdateClusterFilterDataFromChildren(FPBDRigidClusteredParticleHandle* ClusterParent, const TArray<FPBDRigidParticleHandle*>& Children);
} // namespace Chaos
