// Copyright Epic Games, Inc. All Rights Reserved.

#include "SimModule/SimModuleTree.h"
#include "SimModule/SimulationModuleBase.h"
#include "Chaos/ParticleHandleFwd.h"
#include "PhysicsProxy/GeometryCollectionPhysicsProxy.h"
#include "VehicleUtility.h"

#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_DISABLE_OPTIMIZATION
#endif

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

		SimulationModuleTree[OrginalParent].Children.Remove(AtIndex);
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
		Node.Parent = FSimModuleNode::INVALID_INDEX;
	}

	return NewIndex;
}

int FSimModuleTree::GetNextIndex()
{
	int NewIndex = FSimModuleNode::INVALID_INDEX;
	if (FreeList.IsEmpty())
	{
		NewIndex = SimulationModuleTree.Num();
		SimulationModuleTree.AddZeroed(1);
		SimulationModuleTree[NewIndex].Parent = FSimModuleNode::INVALID_INDEX;
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
	int NewIndex = FSimModuleNode::INVALID_INDEX;

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

	SimulationModuleTree[AtIndex].Parent = FSimModuleNode::INVALID_INDEX;
	SimulationModuleTree[AtIndex].Children.Empty();
	delete SimulationModuleTree[AtIndex].SimModule;
	SimulationModuleTree[AtIndex].SimModule = nullptr;

	FreeList.Push(AtIndex);

}

void FSimModuleTree::Simulate(float DeltaTime, FAllInputs& Inputs, FGeometryCollectionPhysicsProxy* PhysicsProxy)
{
	if (PhysicsProxy)
	{
		UpdateModuleVelocites(PhysicsProxy);
	}

	TArray<int> RootNodes;
	GetRootNodes(RootNodes);

	for (int RootIndex : RootNodes)
	{
		SimulateNode(DeltaTime, Inputs, RootIndex);
	}
}

void FSimModuleTree::SimulateNode(float DeltaTime, FAllInputs& Inputs, int NodeIndex)
{
	if (ISimulationModuleBase* Module = AccessSimModule(NodeIndex))
	{
		if (Module->IsEnabled())
		{
			Module->Simulate(DeltaTime, Inputs, *this);
		}

		for (int ChildIdx : GetChildren(NodeIndex))
		{
			SimulateNode(DeltaTime, Inputs, ChildIdx);
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
		SimulationModuleTree[AtIndex].Parent = FSimModuleNode ::INVALID_INDEX;

		FreeList.Push(AtIndex);

	}
}


void FSimModuleTree::GetRootNodes(TArray<int>& RootNodesOut)
{
	RootNodesOut.Empty();

	// never assume the root bone is always index 0
	for (int i = 0; i < SimulationModuleTree.Num(); i++)
	{
		if (SimulationModuleTree[i].SimModule != nullptr && SimulationModuleTree[i].Parent == FSimModuleNode::INVALID_INDEX)
		{
			RootNodesOut.Add(i);
		}
	}
}

void FSimModuleTree::UpdateModuleVelocites(FGeometryCollectionPhysicsProxy* PhysicsProxy)
{
	check(PhysicsProxy);

	const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Clusters = PhysicsProxy->GetSolverClusterHandles();
	const TArray<Chaos::FPBDRigidClusteredParticleHandle*>& Particles = PhysicsProxy->GetSolverParticleHandles();

	// capture the velocities at the start of each sim iteration
	for (int i = 0; i < SimulationModuleTree.Num(); i++)
	{
		if (ISimulationModuleBase* Module = SimulationModuleTree[i].SimModule)
		{
			if (Chaos::FPBDRigidClusteredParticleHandle* ParentParticle = Clusters[Module->GetTransformIndex()])
			{
				const FTransform BodyTransform(ParentParticle->R(), ParentParticle->X());

				if (Module->IsBehaviourType(eSimModuleTypeFlags::Velocity))
				{
					Chaos::FPBDRigidClusteredParticleHandle* Particle = nullptr;
					if (Module->IsClustered())
					{
						Particle = Clusters[Module->GetTransformIndex()];
					}
					else
					{
						Particle = Particles[Module->GetTransformIndex()];				
					}

					if (Particle)
					{
						FVector WorldLocation = BodyTransform.TransformPosition(Module->GetParentRelativeTransform().GetLocation());
						const Chaos::FVec3 Arm = WorldLocation - Particle->X();

						FVector WorldVelocity = Particle->V() - Chaos::FVec3::CrossProduct(Arm, Particle->W());
						FVector LocalVelocity = BodyTransform.InverseTransformVector(WorldVelocity);
						LocalVelocity = Module->GetClusteredTransform().InverseTransformVector(LocalVelocity);

						Module->SetLocalVelocity(LocalVelocity);
					}
			
				}
			}
		}
	}

}


} // namespace Chaos


#if VEHICLE_DEBUGGING_ENABLED
PRAGMA_ENABLE_OPTIMIZATION
#endif
