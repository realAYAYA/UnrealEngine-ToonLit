// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SimModule/DeferredForcesModular.h"
#include "SimModule/SimulationModuleBase.h"


DECLARE_STATS_GROUP(TEXT("ModularVehicle.SimTree"), STATGROUP_ModularVehicleSimTree, STATGROUP_Advanced);

class FGeometryCollectionPhysicsProxy;
namespace Chaos
{
	class ISimulationModuleBase;
	class FClusterUnionPhysicsProxy;
	struct FAllInputs;


	struct FPendingModuleAdds
	{
		FPendingModuleAdds(int ParentIndexIn, ISimulationModuleBase* NewSimModuleIn)
			: ParentIndex(ParentIndexIn), NewSimModule(NewSimModuleIn) {}

		int ParentIndex;
		ISimulationModuleBase* NewSimModule;
	};

	struct FPendingModuleDeletions
	{
		FPendingModuleDeletions(int GuidIn) : Guid(GuidIn) {}
		int Guid;
	};

	// Each update tree has it's own local tree hierarchy, this will be translated into the actual tree hierarchy.
	// The root in here will always translate to the root in the main sim tree
	class FSimTreeUpdates
	{
	public:
		int AddRoot(ISimulationModuleBase* NewSimModuleIn)
		{
			NewModules.Add(FPendingModuleAdds(-1, NewSimModuleIn));
			return NewModules.Num()-1;
		}

		int AddNodeBelow(int ParentIndex, ISimulationModuleBase* NewSimModuleIn)
		{
			NewModules.Add(FPendingModuleAdds(ParentIndex, NewSimModuleIn));
			return NewModules.Num() - 1;
		}

		void RemoveNode(int Guid)
		{
			DeletedModules.Add(FPendingModuleDeletions(Guid));
		}

		void ClearUpdates()
		{
			NewModules.Empty();
			DeletedModules.Empty();
		}

		const TArray<FPendingModuleAdds>& GetNewModules() const { return NewModules; }
		const TArray<FPendingModuleDeletions>& GetDeletedModules() const { return DeletedModules; }

	private:
		TArray<FPendingModuleAdds> NewModules;
		TArray<FPendingModuleDeletions> DeletedModules;
	};

	struct CHAOSVEHICLESCORE_API FVehicleState
	{
		float ForwardSpeedKmh;
		FVector ForwardDir;
		FVector RightDir;
		FVector UpDir;
		FVector AngularVelocityRad;
	};

	class CHAOSVEHICLESCORE_API FSimModuleTree
	{
		friend class FModularVehicleBuilder;

	public:
		struct FSimModuleNode
		{
			FSimModuleNode()
				: SimModule(nullptr)
				, Parent(INVALID_IDX)
			{
			}

			bool IsValid() const { return (SimModule != nullptr); }

			ISimulationModuleBase* SimModule;
			int Parent;
			TSet<int> Children;

			const static int INVALID_IDX = -1;

		};

		FSimModuleTree()
		{
			bAnimationEnabled = true;
			bSimulationEnabled = true;
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
		FSimModuleNode& GetNode(int Index) { return SimulationModuleTree[Index]; }

		int AddNodeBelow(int AtIndex, ISimulationModuleBase* SimModule);

		int InsertNodeAbove(int AtIndex, ISimulationModuleBase* SimModule);

		void DeleteNode(int AtIndex);

		void AppendTreeUpdates(const FSimTreeUpdates& TreeUpdates);

		void Simulate(float DeltaTime, FAllInputs& Inputs, FClusterUnionPhysicsProxy* PhysicsProxy);

		FDeferredForcesModular& AccessDeferredForces() { return DeferredForces; }
		const FDeferredForcesModular& GetDeferredForces() const { return DeferredForces; }
		const TArray<FSimModuleNode>& GetSimulationModuleTree() { return SimulationModuleTree; }

		void SetAnimationEnabled(bool bInEnabled) { bAnimationEnabled = bInEnabled; }
		bool IsAnimationEnabled() { return bAnimationEnabled; }
		void SetSimulationEnabled(bool bInEnabled) { bSimulationEnabled = bInEnabled; }
		bool IsSimulationEnabled() { return bSimulationEnabled; }

		FControlInputs& GetControlInputs()
		{
			return AllInputs.ControlInputs;
		}

		const FVehicleState& GetVehicleState() const
		{
			return VehicleState;
		}

		FSimModuleNode* LocateNodeByType(Chaos::eSimType InType)
		{
			for (FSimModuleNode& Node : SimulationModuleTree)
			{
				if (Node.SimModule && Node.SimModule->GetSimType() == InType)
				{
					return &Node;
				}
			}

			return nullptr;
		}

		int GetLargestComponentIndex()
		{
			int LargestIndex = -1;
			for (int I = 0; I < GetNumNodes(); I++)
			{
				if (Chaos::ISimulationModuleBase* SimModule = GetNode(I).SimModule)
				{
					if (SimModule->GetTransformIndex() > LargestIndex)
					{
						LargestIndex = SimModule->GetTransformIndex();
					}
				}
			}
			return LargestIndex;
		}

		void GenerateReplicationStructure(Chaos::FModuleNetDataArray& NetData);
		void SetNetState(Chaos::FModuleNetDataArray& ModuleDatas);
		void SetSimState(const Chaos::FModuleNetDataArray& ModuleDatas);
		void InterpolateState(const float LerpFactor, Chaos::FModuleNetDataArray& LerpDatas, const Chaos::FModuleNetDataArray& MinDatas, const Chaos::FModuleNetDataArray& MaxDatas);

	protected:
		void SimulateNode(float DeltaTime, FAllInputs& Inputs, int NodeIdx, FClusterUnionPhysicsProxy* PhysicsProxy);

		void DeleteNodesBelow(int NodeIdx);

		int GetNextIndex();

		void UpdateModuleVelocites(FGeometryCollectionPhysicsProxy* PhysicsProxy);
		void UpdateModuleVelocites(FClusterUnionPhysicsProxy* PhysicsProxy, bool bWake);
		void UpdateVehicleState(FClusterUnionPhysicsProxy* PhysicsProxy);

		TArray<FSimModuleNode> SimulationModuleTree;
		TArray<int> FreeList;

		FDeferredForcesModular DeferredForces;

		Chaos::FAllInputs AllInputs;

		bool bAnimationEnabled;
		bool bSimulationEnabled;

		FVehicleState VehicleState;
	};




} // namespace Chaos
