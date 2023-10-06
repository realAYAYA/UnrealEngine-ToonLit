// Copyright Epic Games, Inc. All Rights Reserved.

#include "Chaos/StrainModification.h"
#include "Chaos/PBDRigidClustering.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "GeometryCollection/GeometryCollectionSimulationCoreTypes.h"
#include "GeometryCollection/GeometryCollection.h"

const FGeometryCollectionPhysicsProxy* Chaos::FStrainedProxyModifier::GetProxy() const
{
	return Proxy;
}

const Chaos::FPBDRigidParticleHandle* Chaos::FStrainedProxyModifier::GetRootHandle() const
{
	return RootHandle;
}

int32 Chaos::FStrainedProxyModifier::GetNumRestBreakables() const
{
	if (RestChildren)
	{
		return RestChildren->Num();
	}
	return 0;
}

int32 Chaos::FStrainedProxyModifier::GetNumBreakingStrains(const bool bDoubleCount, const uint8 StrainTypes) const
{
	// Make sure we have a proxy and rest-children
	if (Proxy == nullptr) { return 0; }
	if (RestChildren == nullptr) { return 0; }

	// Loop over each child, checking whether or not it will have been freed
	// by the strain that it has accumulated
	int32 NumBreakingStrains = 0;
	for (int32 RestChildIdx : *RestChildren)
	{
		Chaos::FPBDRigidClusteredParticleHandle* ChildHandle = Proxy->GetSolverParticleHandles()[RestChildIdx];
		if (ChildHandle->Parent() == nullptr) { continue; }

		// Get the applied and internal strain
		//
		// TODO: Make this a computation internal to the strain, or the connection
		// graph or something... this logic is currently copied from
		// FRigidClustering::ReleaseClusterParticlesImpl, but should be tucked behind
		// an interface somewhere.
		const Chaos::FReal CollisionImpulses
			= (StrainTypes & Chaos::EStrainTypes::CollisionStrain)
			? ChildHandle->CollisionImpulses() : FReal(0);
		const Chaos::FReal ExternalStrain
			= (StrainTypes & Chaos::EStrainTypes::ExternalStrain)
			? ChildHandle->GetExternalStrain() : FReal(0);
		const Chaos::FReal InternalStrain = ChildHandle->GetInternalStrains();
		Chaos::FReal MaxAppliedStrain = FMath::Max(CollisionImpulses, ExternalStrain);

		if (bDoubleCount && InternalStrain > SMALL_NUMBER)
		{
			NumBreakingStrains += (int32)(MaxAppliedStrain / InternalStrain);
		}
		else if (MaxAppliedStrain >= InternalStrain)
		{
			++NumBreakingStrains;
		}
	}

	// Return the number of breaking strains
	return NumBreakingStrains;
}

void Chaos::FStrainedProxyModifier::ClearStrains()
{
	// Make sure we have a proxy and rest-children
	if (Proxy == nullptr) { return; }
	if (RestChildren == nullptr) { return; }

	// Loop over each child, clearing the strain associated with it
	for (int32 RestChildIdx : *RestChildren)
	{
		if (Chaos::FPBDRigidClusteredParticleHandle* ChildHandle = Proxy->GetSolverParticleHandles()[RestChildIdx])
		{
			// Clear accumulated collision impulses
			ChildHandle->ClearCollisionImpulse();

			// Clear accumulated external strains
			//
			// NOTE: The following is deprecated:
			// ChildHandle->ClearExternalStrain();
			// Instead we use:
			RigidClustering.SetExternalStrain(ChildHandle, FReal(0));
		}
	}
}

Chaos::FPBDRigidClusteredParticleHandle* Chaos::FStrainedProxyModifier::InitRootHandle(FGeometryCollectionPhysicsProxy* Proxy)
{
	if (Proxy == nullptr) { return nullptr; }

	// Get the root index of the proxy
	FSimulationParameters& Parameters = Proxy->GetSimParameters();
	const int32 RootIndex = Parameters.InitialRootIndex;
	if (RootIndex == INDEX_NONE) { return nullptr; }

	// Return the particle handle corresponding to the root index
	TArray<Chaos::FPBDRigidClusteredParticleHandle*>& ParticleHandles = Proxy->GetSolverParticleHandles();
	if (!ParticleHandles.IsValidIndex(RootIndex)) { return nullptr; }
	return ParticleHandles[RootIndex];
}

const TSet<int32>* Chaos::FStrainedProxyModifier::InitRestChildren(FGeometryCollectionPhysicsProxy* Proxy)
{
	if (Proxy == nullptr) { return nullptr; }

	// Get the root index of the proxy
	FSimulationParameters& Parameters = Proxy->GetSimParameters();
	const int32 RootIndex = Parameters.InitialRootIndex;
	if (RootIndex == INDEX_NONE) { return nullptr; }

	// Return a ptr to the set of child indices
	return &Parameters.RestCollection->Children[RootIndex];
}

Chaos::FStrainedProxyModifier Chaos::FStrainedProxyIterator::operator*()
{
	return Chaos::FStrainedProxyModifier(RigidClustering, Proxies[Index]);
}

Chaos::FStrainedProxyIterator& Chaos::FStrainedProxyIterator::operator++()
{
	++Index;
	return *this;
}

bool Chaos::FStrainedProxyIterator::operator==(const FStrainedProxyIterator& Other) const
{
	return
		Proxies == Other.Proxies &&
		Index == Other.Index;
}

Chaos::FStrainedProxyRange::FStrainedProxyRange(Chaos::FRigidClustering& InRigidClustering, const bool bRootLevelOnly)
	: RigidClustering(InRigidClustering)
{
	const TSet<Chaos::FPBDRigidClusteredParticleHandle*>& StrainedParents = RigidClustering.GetTopLevelClusterParentsStrained();

	Proxies.Reserve(StrainedParents.Num());
	for (Chaos::FPBDRigidClusteredParticleHandle* Cluster : StrainedParents)
	{
		// Make sure the cluster's physics proxy exists and is the right type
		IPhysicsProxyBase* ProxyBase = Cluster->PhysicsProxy();
		if (ProxyBase == nullptr)
		{
			continue;
		}

		if (ProxyBase->GetType() != EPhysicsProxyType::GeometryCollectionType)
		{
			continue;
		}

		FGeometryCollectionPhysicsProxy* Proxy = static_cast<FGeometryCollectionPhysicsProxy*>(ProxyBase);

		// Make sure the rest collection has a root index
		if (bRootLevelOnly)
		{
			FSimulationParameters& Parameters = Proxy->GetSimParameters();
			const int32 RootIndex = Parameters.InitialRootIndex;
			TArray<Chaos::FPBDRigidClusteredParticleHandle*>& ParticleHandles = Proxy->GetSolverParticleHandles();
			if (ParticleHandles.IsValidIndex(RootIndex))
			{
				if (Cluster != ParticleHandles[RootIndex])
				{
					continue;
				}
			}
		}

		// Only need to use AddUnique if we're not checking for root, since at most
		// one cluster will have the rest collection's root index.
		if (bRootLevelOnly)
		{
			Proxies.Add(Proxy);
		}
		else
		{
			Proxies.AddUnique(Proxy);
		}
	}
}

Chaos::FStrainedProxyRange Chaos::FStrainModifierAccessor::GetStrainedProxies(const bool bRootLevelOnly)
{
	return FStrainedProxyRange(RigidClustering, bRootLevelOnly);
}

