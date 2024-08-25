// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/SimulationModuleBase.h"
#include "SimModule/SimModuleTree.h"
#include "SimModule/DeferredForcesModular.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"

DEFINE_LOG_CATEGORY(LogSimulationModule);

namespace Chaos
{

void ISimulationModuleBase::AddLocalForceAtPosition(const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce, bool bLevelSlope, const FColor& DebugColorIn)
{
	AppliedForce = Force;
	if (SimModuleTree)
	{
		SimModuleTree->AccessDeferredForces().Add(FDeferredForcesModular::FApplyForceAtPositionData(ComponentTransform, TransformIndex, ParticleIdx.Idx, Force, Position, bAllowSubstepping, bIsLocalForce, bLevelSlope, DebugColorIn));
	}
}

void ISimulationModuleBase::AddLocalForce(const FVector& Force, bool bAllowSubstepping, bool bIsLocalForce, bool bLevelSlope, const FColor& DebugColorIn)
{
	AppliedForce = Force;
	if (SimModuleTree)
	{
		SimModuleTree->AccessDeferredForces().Add(FDeferredForcesModular::FApplyForceData(ComponentTransform, TransformIndex, ParticleIdx.Idx, Force, bAllowSubstepping, bIsLocalForce, bLevelSlope, DebugColorIn));
	}
}

void ISimulationModuleBase::AddLocalTorque(const FVector& Torque, bool bAllowSubstepping, bool bAccelChangeIn, const FColor& DebugColorIn)
{
	if (SimModuleTree)
	{
		SimModuleTree->AccessDeferredForces().Add(FDeferredForcesModular::FAddTorqueInRadiansData(ComponentTransform, TransformIndex, ParticleIdx.Idx, Torque, bAllowSubstepping, bAccelChangeIn, DebugColorIn));
	}
}

ISimulationModuleBase* ISimulationModuleBase::GetParent()
{
	return (SimModuleTree != nullptr) ? SimModuleTree->AccessSimModule(SimModuleTree->GetParent(SimTreeIndex)) : nullptr;
}

ISimulationModuleBase* ISimulationModuleBase::GetFirstChild()
{
	if (SimModuleTree)
	{
		const TSet<int32>& Children = SimModuleTree->GetChildren(SimTreeIndex);
		for (const int ChildIndex : Children)
		{ 
			return SimModuleTree->AccessSimModule(ChildIndex);
		}
	}
	return nullptr;
}

FPBDRigidClusteredParticleHandle* ISimulationModuleBase::GetClusterParticle(Chaos::FClusterUnionPhysicsProxy* Proxy)
{ 
	// TODO: should store what we need rather than search for it all the time
	FPBDRigidClusteredParticleHandle* ClusterChild = nullptr;

	FPBDRigidsEvolutionGBF& Evolution = *static_cast<FPBDRigidsSolver*>(Proxy->GetSolver<FPBDRigidsSolver>())->GetEvolution();
	FClusterUnionManager& ClusterUnionManager = Evolution.GetRigidClustering().GetClusterUnionManager();
	const FClusterUnionIndex& CUI = Proxy->GetClusterUnionIndex();

	if (FClusterUnion* ClusterUnion = ClusterUnionManager.FindClusterUnion(CUI))
	{
		FPBDRigidClusteredParticleHandle* ClusterHandle = ClusterUnion->InternalCluster;
		TArray<FPBDRigidParticleHandle*> Particles = ClusterUnion->ChildParticles;

		if (FPBDRigidParticleHandle* Particle = GetParticleFromUniqueIndex(ParticleIdx.Idx, Particles))
		{
			ClusterChild = Particle->CastToClustered();
		}
	}

	return ClusterChild;
}

FPBDRigidParticleHandle* ISimulationModuleBase::GetParticleFromUniqueIndex(int32 ParticleUniqueIdx, TArray<FPBDRigidParticleHandle*>& Particles)
{
	for (FPBDRigidParticleHandle* Particle : Particles)
	{
		if (Particle && Particle->UniqueIdx().IsValid())
		{
			if (ParticleUniqueIdx == Particle->UniqueIdx().Idx)
			{
				return Particle;
			}
		}
	}

	return nullptr;
}

bool ISimulationModuleBase::GetDebugString(FString& StringOut) const 
{
	StringOut += FString::Format(TEXT("{0}: TreeIndex {1}, Enabled {2}, InCluster {3}, TFormIdx {4}, ")
		, { GetDebugName(), GetTreeIndex(), IsEnabled(), IsClustered(), GetTransformIndex() });

	return true; 
}

const FTransform& ISimulationModuleBase::GetParentRelativeTransform() const
{
	if (bClustered)
	{
		return GetClusteredTransform();
	}
	else
	{
		return GetIntactTransform();
	}
}


void FSimOutputData::FillOutputState(const ISimulationModuleBase* SimModule)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (SimModule)
	{	
		DebugString.Empty();
		SimModule->GetDebugString(DebugString);
	}
#endif
}


} //namespace Chaos