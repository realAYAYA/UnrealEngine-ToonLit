// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/SimModuleTree.h"
#include "Chaos/ParticleHandleFwd.h"
#include "Chaos/GeometryParticlesfwd.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "PhysicsProxy/ClusterUnionPhysicsProxy.h"
#include "Chaos/PhysicsObjectInternalInterface.h"
#include "Chaos/DebugDrawQueue.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
UE_DISABLE_OPTIMIZATION_SHIP
#endif

DECLARE_CYCLE_STAT(TEXT("ModularVehicle_SimulateTree"), STAT_ModularVehicle_SimulateTree, STATGROUP_ModularVehicleSimTree);
DECLARE_CYCLE_STAT(TEXT("ModularVehicle_GenerateReplicationStructure"), STAT_ModularVehicle_GenerateReplicationStructure, STATGROUP_ModularVehicleSimTree);
DECLARE_CYCLE_STAT(TEXT("ModularVehicle_SetNetState"), STAT_ModularVehicle_SetNetState, STATGROUP_ModularVehicleSimTree);
DECLARE_CYCLE_STAT(TEXT("ModularVehicle_SetSimState"), STAT_ModularVehicle_SetSimState, STATGROUP_ModularVehicleSimTree);
DECLARE_CYCLE_STAT(TEXT("ModularVehicle_AppendTreeUpdates"), STAT_ModularVehicle_AppendTreeUpdates, STATGROUP_ModularVehicleSimTree);

bool bModularVehicle_NetworkData_Enable = true;
FAutoConsoleVariableRef CVarModularVehicleNetworkDataEnable(TEXT("p.ModularVehicle.NetworkData.Enable"), bModularVehicle_NetworkData_Enable, TEXT("Enable/Disable additional module network data."));

bool bModularVehicle_DisableAllSimulationAfterDestruction_Enable = false;
FAutoConsoleVariableRef CVarModularVehicleDisableAllSimulationAfterDestruction(TEXT("p.ModularVehicle.DisableAllSimulationAfterDestruction.Enable"), bModularVehicle_DisableAllSimulationAfterDestruction_Enable, TEXT("Enable/Disable whole vehicle simulation after destruction has occured."));

namespace Chaos
{

int FSimModuleTree::AddRoot(ISimulationModuleBase* SimModule)
{
	return AddNodeBelow(-1, SimModule);
}

void FSimModuleTree::Reparent(int AtIndex, int ParentIndex)
{
	check(AtIndex < SimulationModuleTree.Num());
	check(ParentIndex < SimulationModuleTree.Num());

	UE_LOG(LogSimulationModule, Log, TEXT("Reparent %s To %s")
		, *SimulationModuleTree[AtIndex].SimModule->GetDebugName()
		, *SimulationModuleTree[ParentIndex].SimModule->GetDebugName());

	int OrginalParent = SimulationModuleTree[AtIndex].Parent;
	if (OrginalParent != ParentIndex)
	{
		SimulationModuleTree[AtIndex].Parent = ParentIndex;
		SimulationModuleTree[ParentIndex].Children.Add(AtIndex);

		// if had a parent and wasn't a root
		if (OrginalParent != -1)
		{
			SimulationModuleTree[OrginalParent].Children.Remove(AtIndex);
		}
	}
}

int FSimModuleTree::AddNodeBelow(int AtIndex, ISimulationModuleBase* SimModule)
{
	int NewIndex = GetNextIndex();
	FSimModuleNode& Node = SimulationModuleTree[NewIndex];
	SimModule->SetTreeIndex(NewIndex);
	Node.SimModule = SimModule;
	Node.Parent = AtIndex;
	if (AtIndex >= 0)
	{
		SimulationModuleTree[AtIndex].Children.Add(NewIndex);
	}
	else
	{
		Node.Parent = FSimModuleNode::INVALID_IDX;
	}

	return NewIndex;
}

void FSimModuleTree::AppendTreeUpdates(const FSimTreeUpdates& TreeUpdates)
{
	SCOPE_CYCLE_COUNTER(STAT_ModularVehicle_AppendTreeUpdates);

	int TreeIndex = -1;
	TMap<int, int> SimTreeMapping;

	//if (GetNumNodes() == 0) //Always add a null node, so there is a parent when the root chassis is removed
	//{
	//	// add a single chassis root component
	//	Chaos::FChassisSettings Settings;
	//	Chaos::ISimulationModuleBase* Chassis = new Chaos::FChassisSimModule(Settings);
	//	ParentIndex = AddRoot(Chassis);
	//	Chassis->SetTransformIndex(-1);
	//}

	int LocalIndex = 0;
	for (const FPendingModuleAdds& TreeUpdate : TreeUpdates.GetNewModules())
	{
		int AddIndex = -1;
		if (int* AddIndexPtr = SimTreeMapping.Find(TreeUpdate.ParentIndex))
		{
			AddIndex = *AddIndexPtr;
		}

		TreeIndex = AddNodeBelow(AddIndex, TreeUpdate.NewSimModule);
		SimTreeMapping.Add(LocalIndex, TreeIndex);
		LocalIndex++;
	}

	TArray<int> ComponentIndices;

	if (!SimulationModuleTree.IsEmpty())
	{
		for (const FPendingModuleDeletions& TreeUpdate : TreeUpdates.GetDeletedModules())
		{
			for (int Index = 0; Index < SimulationModuleTree.Num(); Index++)
			{
				if (Chaos::ISimulationModuleBase* SimModule = GetNode(Index).SimModule)
				{
					if (SimModule->GetGuid() == TreeUpdate.Guid)
					{
						ComponentIndices.AddUnique(SimModule->GetTransformIndex());
						DeleteNode(Index);
						break;
					}
				}
			}
		}				
	}

}


int FSimModuleTree::GetNextIndex()
{
	int NewIndex = FSimModuleNode::INVALID_IDX;
	if (FreeList.IsEmpty())
	{
		NewIndex = SimulationModuleTree.Num();
		SimulationModuleTree.AddZeroed(1);
		SimulationModuleTree[NewIndex].Parent = FSimModuleNode::INVALID_IDX;
		SimulationModuleTree[NewIndex].SimModule = nullptr;
	}
	else
	{
		NewIndex = FreeList.Pop();
	}

	return NewIndex;
}

int FSimModuleTree::InsertNodeAbove(int AtIndex, ISimulationModuleBase* SimModule)
{
	int NewIndex = FSimModuleNode::INVALID_IDX;

	if (ensure(AtIndex < SimulationModuleTree.Num()))
	{
		NewIndex = GetNextIndex();
		FSimModuleNode& Node = SimulationModuleTree[NewIndex];

		int OriginalParentIdx = SimulationModuleTree[AtIndex].Parent;

		// remove current idx from children of parent & add new index in its place
		SimulationModuleTree[OriginalParentIdx].Children.Remove(AtIndex);
		SimulationModuleTree[OriginalParentIdx].Children.Add(NewIndex);

		SimulationModuleTree[AtIndex].Parent = NewIndex;

		Node.SimModule = SimModule;
		Node.Parent = OriginalParentIdx;	// new node takes parent from existing node
		Node.Children.Add(AtIndex);			// existing node becomes child of new node
		SimModule->SetTreeIndex(NewIndex);
	}

	return NewIndex;
}

void FSimModuleTree::DeleteNode(int AtIndex)
{
	// if is there is ever an issue then we have the option of disabling ALL module simulation after first destruction occurs
	if (bModularVehicle_DisableAllSimulationAfterDestruction_Enable)
	{
		SetSimulationEnabled(false);
	}

	// multiple children might become equal parents?
	
	int ParentIndex = SimulationModuleTree[AtIndex].Parent;
	
	if (ParentIndex >= 0)
	{
		// remove from parents children list
		SimulationModuleTree[ParentIndex].Children.Remove(AtIndex);
	}

	// move deleted nodes children to parent and these children need new parent
	for (int ChildIndex : SimulationModuleTree[AtIndex].Children)
	{
		if (ParentIndex >= 0)
		{
			SimulationModuleTree[ParentIndex].Children.Add(ChildIndex);
		}
		SimulationModuleTree[ChildIndex].Parent = ParentIndex;
	}	

	SimulationModuleTree[AtIndex].Parent = FSimModuleNode::INVALID_IDX;
	SimulationModuleTree[AtIndex].Children.Empty();
	delete SimulationModuleTree[AtIndex].SimModule;
	SimulationModuleTree[AtIndex].SimModule = nullptr;

	FreeList.Push(AtIndex);

}


void FSimModuleTree::Simulate(float DeltaTime, FAllInputs& Inputs, FClusterUnionPhysicsProxy* PhysicsProxy)
{
	SCOPE_CYCLE_COUNTER(STAT_ModularVehicle_SimulateTree);

	if (IsSimulationEnabled())
	{
		if (PhysicsProxy)
		{
			UpdateVehicleState(PhysicsProxy);

			UpdateModuleVelocites(PhysicsProxy, Inputs.ControlInputs.InputNonZero() || Inputs.bKeepVehicleAwake);
		}

		TArray<int> RootNodes;
		GetRootNodes(RootNodes);

		for (int RootIndex : RootNodes)
		{
			SimulateNode(DeltaTime, Inputs, RootIndex, PhysicsProxy);
		}
	}

}

void FSimModuleTree::SimulateNode(float DeltaTime, FAllInputs& Inputs, int NodeIndex, FClusterUnionPhysicsProxy* PhysicsProxy)
{
	if (ISimulationModuleBase* Module = AccessSimModule(NodeIndex))
	{
		if (Module->IsEnabled())
		{
			Module->Simulate(DeltaTime, Inputs, *this);

			if (IsAnimationEnabled() && Module->IsAnimationEnabled())
			{
				Module->Animate(PhysicsProxy);
			}
		}

		for (int ChildIdx : GetChildren(NodeIndex))
		{
			SimulateNode(DeltaTime, Inputs, ChildIdx, PhysicsProxy);
		}
	}
}

void FSimModuleTree::DeleteNodesBelow(int AtIndex)
{
	if (IsValidNode(AtIndex))
	{
		for (int ChildIdx : GetChildren(AtIndex))
		{
			DeleteNodesBelow(ChildIdx);
		}

		delete SimulationModuleTree[AtIndex].SimModule;
		SimulationModuleTree[AtIndex].SimModule = nullptr;
		SimulationModuleTree[AtIndex].Children.Empty();
		SimulationModuleTree[AtIndex].Parent = FSimModuleNode ::INVALID_IDX;

		FreeList.Push(AtIndex);

	}
}


void FSimModuleTree::GetRootNodes(TArray<int>& RootNodesOut)
{
	RootNodesOut.Empty();

	// never assume the root bone is always index 0
	for (int i = 0; i < SimulationModuleTree.Num(); i++)
	{
		if (SimulationModuleTree[i].SimModule != nullptr && SimulationModuleTree[i].Parent == FSimModuleNode::INVALID_IDX)
		{
			RootNodesOut.Add(i);
		}
	}
}

void FSimModuleTree::UpdateModuleVelocites(FGeometryCollectionPhysicsProxy* PhysicsProxy)
{
	check(PhysicsProxy);

	// capture the velocities at the start of each sim iteration
	for (int i = 0; i < SimulationModuleTree.Num(); i++)
	{
		if (ISimulationModuleBase* Module = SimulationModuleTree[i].SimModule)
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ParentParticle = PhysicsProxy->GetSolverClusterHandle_Internal(Module->GetTransformIndex()))
			{
				const FTransform BodyTransform(ParentParticle->GetR(), ParentParticle->GetX());

				if (Module->IsBehaviourType(eSimModuleTypeFlags::Velocity))
				{
					Chaos::FPBDRigidClusteredParticleHandle* Particle = nullptr;
					if (Module->IsClustered())
					{
						Particle = PhysicsProxy->GetSolverClusterHandle_Internal(Module->GetTransformIndex());
					}
					else
					{
						Particle = PhysicsProxy->GetParticle_Internal(Module->GetTransformIndex());
					}

					if (Particle)
					{
						FVector WorldLocation = BodyTransform.TransformPosition(Module->GetParentRelativeTransform().GetLocation());
						const Chaos::FVec3 Arm = WorldLocation - Particle->GetX();

						FVector WorldVelocity = Particle->GetV() - Chaos::FVec3::CrossProduct(Arm, Particle->GetW());
						FVector LocalLinearVelocity = BodyTransform.InverseTransformVector(WorldVelocity);
						LocalLinearVelocity = Module->GetClusteredTransform().InverseTransformVector(LocalLinearVelocity);

						FVector LocalAngular = BodyTransform.InverseTransformVector(Particle->GetW());
						LocalAngular = Module->GetClusteredTransform().InverseTransformVector(LocalAngular);

						Module->SetLocalLinearVelocity(LocalLinearVelocity);
						Module->SetLocalAngularVelocity(LocalAngular);
					}
			
				}
			}
		}
	}

}

void FSimModuleTree::UpdateModuleVelocites(FClusterUnionPhysicsProxy* PhysicsProxy, bool bWake)
{
	check(PhysicsProxy);
	Chaos::EnsureIsInPhysicsThreadContext();

	if (Chaos::FClusterUnionPhysicsProxy::FInternalParticle* ParentParticle = PhysicsProxy->GetParticle_Internal())
	{
		if (bWake && !SimulationModuleTree.IsEmpty())
		{
			const FPhysicsObjectHandle& PhysicsObject = PhysicsProxy->GetPhysicsObjectHandle();
			
			Chaos::FWritePhysicsObjectInterface_Internal WriteInterface = Chaos::FPhysicsObjectInternalInterface::GetWrite();
			WriteInterface.WakeUp( { &PhysicsObject, 1 });
		}

		// capture the velocities at the start of each sim iteration
		for (int i = 0; i < SimulationModuleTree.Num(); i++)
		{
			if (ISimulationModuleBase* Module = SimulationModuleTree[i].SimModule)
			{
				const FTransform BodyTransform(ParentParticle->GetR(), ParentParticle->GetX());

				if (Module->IsBehaviourType(eSimModuleTypeFlags::Velocity))
				{
					const Chaos::FClusterUnionPhysicsProxy::FInternalParticle* Particle = nullptr;
					if (Module->IsClustered())
					{
						Particle = ParentParticle;
					}
					else
					{
						//				Particle = Particles[Module->GetTransformIndex()];
					}

					if (Particle)
					{
						const FTransform& OffsetTransform = Module->GetComponentTransform();
						FVector LocalPos = Module->GetParentRelativeTransform().GetLocation();
						FVector WorldLocation = BodyTransform.TransformPosition(LocalPos);
						const Chaos::FVec3 Arm = WorldLocation - Particle->GetX();

						//Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(Particle->X(), Particle->X() + Arm, FColor::Yellow, false, -1.f, 0, 2.f);

						FVector WorldVelocity = Particle->GetV() - Chaos::FVec3::CrossProduct(Arm, Particle->GetW());
						FVector LocalVelocity = OffsetTransform.InverseTransformVector(BodyTransform.InverseTransformVector(WorldVelocity));

						FVector LocalAngular = BodyTransform.InverseTransformVector(Particle->GetW());
						LocalAngular = Module->GetClusteredTransform().InverseTransformVector(LocalAngular);

						Module->SetLocalLinearVelocity(LocalVelocity);
						Module->SetLocalAngularVelocity(LocalAngular);

						//Chaos::FDebugDrawQueue::GetInstance().DrawDebugLine(WorldLocation, WorldLocation + WorldVelocity, FColor::White, false, -1.f, 0, 5.f);

					}

				}
			}
		}
	}
}

void FSimModuleTree::UpdateVehicleState(FClusterUnionPhysicsProxy* PhysicsProxy)
{
	check(PhysicsProxy);
	Chaos::EnsureIsInPhysicsThreadContext();

	if (Chaos::FClusterUnionPhysicsProxy::FInternalParticle* ParentParticle = PhysicsProxy->GetParticle_Internal())
	{
		const FTransform BodyTransform(ParentParticle->GetR(), ParentParticle->GetX());

		VehicleState.ForwardDir = BodyTransform.GetUnitAxis(EAxis::X);
		VehicleState.UpDir = BodyTransform.GetUnitAxis(EAxis::Z);
		VehicleState.RightDir = BodyTransform.GetUnitAxis(EAxis::Y);
		VehicleState.ForwardSpeedKmh = Chaos::CmSToKmH(FVector::DotProduct(ParentParticle->GetV(), VehicleState.ForwardDir));
		VehicleState.AngularVelocityRad = ParentParticle->GetW();
	}

}

void FSimModuleTree::GenerateReplicationStructure(Chaos::FModuleNetDataArray& NetData)
{
	SCOPE_CYCLE_COUNTER(STAT_ModularVehicle_GenerateReplicationStructure);

	if (!bModularVehicle_NetworkData_Enable)
	{
		return;
	}

	const TArray<FSimModuleNode>& Tree = SimulationModuleTree;
	NetData.Reserve(Tree.Num());
	for (int Index = 0; Index < Tree.Num(); Index++)
	{
		if (ISimulationModuleBase* SimModule = Tree[Index].SimModule)
		{
			TSharedPtr<FModuleNetData>&& Data = SimModule->GenerateNetData(Index);
			// not all modules will have net replication data - nullptr is a valid response
			if (Data)
			{
				NetData.Emplace(Data);
			}
		}
	}
}

void FSimModuleTree::SetNetState(Chaos::FModuleNetDataArray& ModuleDatas)
{
	SCOPE_CYCLE_COUNTER(STAT_ModularVehicle_SetNetState);

	if (!bModularVehicle_NetworkData_Enable)
	{
		return;
	}

	if (ModuleDatas.IsEmpty())
	{
		GenerateReplicationStructure(ModuleDatas);
	}

	for (TSharedPtr<FModuleNetData>& DataElement : ModuleDatas)
	{
		if (!SimulationModuleTree.IsEmpty() && DataElement->SimArrayIndex < SimulationModuleTree.Num())
		{
			if (ISimulationModuleBase* SimModule = SimulationModuleTree[DataElement->SimArrayIndex].SimModule)
			{
				DataElement->FillNetState(SimModule);
			}
		}
	}
}

void FSimModuleTree::SetSimState(const Chaos::FModuleNetDataArray& ModuleDatas)
{
	SCOPE_CYCLE_COUNTER(STAT_ModularVehicle_SetSimState);

	if (!bModularVehicle_NetworkData_Enable)
	{
		return;
	}

	for (const TSharedPtr<FModuleNetData>& DataElement : ModuleDatas)
	{
		if (!SimulationModuleTree.IsEmpty() && DataElement->SimArrayIndex < SimulationModuleTree.Num())
		{
			if (ISimulationModuleBase* SimModule = SimulationModuleTree[DataElement->SimArrayIndex].SimModule)
			{
				DataElement->FillSimState(SimModule);
			}
		}
	}
}


void FSimModuleTree::InterpolateState(const float LerpFactor, Chaos::FModuleNetDataArray& LerpDatas, const Chaos::FModuleNetDataArray& MinDatas, const Chaos::FModuleNetDataArray& MaxDatas)
{

}

} // namespace Chaos


#if VEHICLE_DEBUGGING_ENABLED
UE_ENABLE_OPTIMIZATION_SHIP
#endif

