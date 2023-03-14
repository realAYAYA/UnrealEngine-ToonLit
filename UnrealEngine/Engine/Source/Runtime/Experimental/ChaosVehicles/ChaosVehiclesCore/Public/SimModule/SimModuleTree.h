// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/DeferredForcesModular.h"

class FGeometryCollectionPhysicsProxy;

namespace Chaos
{
	class ISimulationModuleBase;
	struct FAllInputs;  // #TODO - needs template or bass class

	class CHAOSVEHICLESCORE_API FSimModuleTree
	{
	public:
		struct FSimModuleNode
		{
			FSimModuleNode()
				: SimModule(nullptr)
				, Parent(INVALID_INDEX)
			{
			}

			ISimulationModuleBase* SimModule;	// we own the pointer, or using sim module container?
			int Parent;
			TSet<int> Children;

			const static int INVALID_INDEX = -1;

		};

		FSimModuleTree()
		{
		}

		~FSimModuleTree()
		{
			DeleteNodesBelow(0);
		}

		void Reset()
		{
			DeleteNodesBelow(0);
		}

		bool IsEmpty() const { return SimulationModuleTree.IsEmpty(); }
		int GetParent(int Index) const { check(!SimulationModuleTree.IsEmpty()); return SimulationModuleTree[Index].Parent; }
		const TSet<int>& GetChildren(int Index) const { check(!SimulationModuleTree.IsEmpty()); return SimulationModuleTree[Index].Children; }
		const ISimulationModuleBase* GetSimModule(int Index) const { return IsValidNode(Index) ? SimulationModuleTree[Index].SimModule : nullptr; }
		ISimulationModuleBase* AccessSimModule(int Index) const { return IsValidNode(Index) ? SimulationModuleTree[Index].SimModule : nullptr; }
		bool IsValidNode(int Index) const { return (SimulationModuleTree.IsEmpty() || Index >= SimulationModuleTree.Num()) ? false : true; }
		int NumActiveNodes() const { return SimulationModuleTree.Num() - FreeList.Num(); }
		void GetRootNodes(TArray<int>& RootNodesOut);
		int GetNumNodes() const { return SimulationModuleTree.Num(); }

		int AddRoot(ISimulationModuleBase* SimModule);

		void Reparent(int Index, int ParentIndex);

		int AddNodeBelow(int AtIndex, ISimulationModuleBase* SimModule);

		int InsertNodeAbove(int AtIndex, ISimulationModuleBase* SimModule);

		void DeleteNode(int AtIndex);

		void Simulate(float DeltaTime, FAllInputs& Inputs, FGeometryCollectionPhysicsProxy* PhysicsProxy);

		FDeferredForcesModular& AccessDeferredForces() { return DeferredForces; }
		const FDeferredForcesModular& GetDeferredForces() const { return DeferredForces; }
		const TArray<FSimModuleNode>& GetSimulationModuleTree() { return SimulationModuleTree; }

	protected:
		void SimulateNode(float DeltaTime, FAllInputs& Inputs, int NodeIdx);

		void DeleteNodesBelow(int NodeIdx);

		int GetNextIndex();

		void UpdateModuleVelocites(FGeometryCollectionPhysicsProxy* PhysicsProxy);

	protected:

		TArray<FSimModuleNode> SimulationModuleTree;
		TArray<int> FreeList;

		FDeferredForcesModular DeferredForces;

	};




} // namespace Chaos
