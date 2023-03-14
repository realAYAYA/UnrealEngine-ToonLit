// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Chaos/PBDRigidClustering.h"

namespace Chaos
{
	void UpdateClusterMassProperties(
		FPBDRigidClusteredParticleHandle* Parent,
		TSet<FPBDRigidParticleHandle*>& Children,
		FMatrix33& ParentInertia,
		const FRigidTransform3* ForceMassOrientation = nullptr);

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
