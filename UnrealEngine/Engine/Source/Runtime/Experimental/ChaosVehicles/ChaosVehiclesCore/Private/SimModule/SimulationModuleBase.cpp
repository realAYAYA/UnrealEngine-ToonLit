// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/SimulationModuleBase.h"
#include "SimModule/SimModuleTree.h"
#include "SimModule/DeferredForcesModular.h"

DEFINE_LOG_CATEGORY(LogSimulationModule);

FCoreModularVehicleDebugParams GModularVehicleDebugParams;

namespace Chaos
{

void ISimulationModuleBase::AddLocalForceAtPosition(const FVector& Force, const FVector& Position, bool bAllowSubstepping, bool bIsLocalForce, bool bLevelSlope, const FColor& DebugColorIn)
{
	AppliedForce = Force;
	if (SimModuleTree)
	{
		SimModuleTree->AccessDeferredForces().Add(FDeferredForcesModular::FApplyForceAtPositionData(TransformIndex, Force, Position, bAllowSubstepping, bIsLocalForce, bLevelSlope, DebugColorIn));
	}
}

void ISimulationModuleBase::AddLocalForce(const FVector& Force, bool bAllowSubstepping, bool bIsLocalForce, bool bLevelSlope, const FColor& DebugColorIn)
{
	AppliedForce = Force;
	if (SimModuleTree)
	{
		SimModuleTree->AccessDeferredForces().Add(FDeferredForcesModular::FApplyForceData(TransformIndex, Force, bAllowSubstepping, bIsLocalForce, bLevelSlope, DebugColorIn));
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

bool ISimulationModuleBase::GetDebugString(FString& StringOut) const 
{
	StringOut += FString::Format(TEXT("{0}: Enabled {1}, InCluster {2}, TFormIdx {3}, ")
		, { GetDebugName(), IsEnabled(), IsClustered(), GetTransformIndex() });

	return true; 
}


} //namespace Chaos