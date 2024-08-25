// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraphInstance.h"

#include "AnimNextStats.h"
#include "DecoratorBase/ExecutionContext.h"
#include "Graph/AnimNextGraph.h"
#include "Graph/GC_GraphInstanceComponent.h"
#include "Graph/RigUnit_AnimNextShimRoot.h"

DEFINE_STAT(STAT_AnimNext_Graph_RigVM);

FAnimNextGraphInstance::~FAnimNextGraphInstance()
{
	Release();
}

void FAnimNextGraphInstance::Release()
{
	if (!GraphInstancePtr.IsValid())
	{
		return;
	}

	GraphInstancePtr.Reset();
	ParentGraphInstance = nullptr;
	RootGraphInstance = nullptr;
	ExtendedExecuteContext.Reset();
	Components.Empty();
	Graph = nullptr;
}

bool FAnimNextGraphInstance::IsValid() const
{
	return GraphInstancePtr.IsValid();
}

const UAnimNextGraph* FAnimNextGraphInstance::GetGraph() const
{
	return Graph;
}

FName FAnimNextGraphInstance::GetEntryPoint() const
{
	return EntryPoint;
}

UE::AnimNext::FWeakDecoratorPtr FAnimNextGraphInstance::GetGraphRootPtr() const
{
	return GraphInstancePtr;
}

FAnimNextGraphInstance* FAnimNextGraphInstance::GetParentGraphInstance() const
{
	return ParentGraphInstance;
}

FAnimNextGraphInstance* FAnimNextGraphInstance::GetRootGraphInstance() const
{
	return RootGraphInstance;
}

bool FAnimNextGraphInstance::UsesGraph(const UAnimNextGraph* InGraph) const
{
	return Graph == InGraph;
}

bool FAnimNextGraphInstance::UsesEntryPoint(FName InEntryPoint) const
{
	if(Graph != nullptr)
	{
		if(InEntryPoint == NAME_None)
		{
			return EntryPoint == Graph->DefaultEntryPoint;
		}

		return InEntryPoint == EntryPoint;
	}
	return false;
}

bool FAnimNextGraphInstance::IsRoot() const
{
	return this == RootGraphInstance;
}

void FAnimNextGraphInstance::AddStructReferencedObjects(FReferenceCollector& Collector)
{
	if (!IsRoot())
	{
		return;	// If we aren't the root graph instance, we don't own the components
	}

	if (const UE::AnimNext::FGCGraphInstanceComponent* Component = TryGetComponent<UE::AnimNext::FGCGraphInstanceComponent>())
	{
		Component->AddReferencedObjects(Collector);
	}
}

UE::AnimNext::FGraphInstanceComponent* FAnimNextGraphInstance::TryGetComponent(int32 ComponentNameHash, FName ComponentName) const
{
	if (const TSharedPtr<UE::AnimNext::FGraphInstanceComponent>* Component = RootGraphInstance->Components.FindByHash(ComponentNameHash, ComponentName))
	{
		return Component->Get();
	}

	return nullptr;
}

UE::AnimNext::FGraphInstanceComponent& FAnimNextGraphInstance::AddComponent(int32 ComponentNameHash, FName ComponentName, TSharedPtr<UE::AnimNext::FGraphInstanceComponent>&& Component)
{
	return *RootGraphInstance->Components.AddByHash(ComponentNameHash, ComponentName, MoveTemp(Component)).Get();
}

GraphInstanceComponentMapType::TConstIterator FAnimNextGraphInstance::GetComponentIterator() const
{
	return RootGraphInstance->Components.CreateConstIterator();
}

void FAnimNextGraphInstance::ExecuteLatentPins(const TConstArrayView<UE::AnimNext::FLatentPropertyHandle>& LatentHandles, void* DestinationBasePtr, bool bIsFrozen)
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph_RigVM);

	if (!IsValid())
	{
		return;
	}

	if (URigVM* VM = Graph->VM)
	{
		FAnimNextExecuteContext& AnimNextContext = ExtendedExecuteContext.GetPublicDataSafe<FAnimNextExecuteContext>();
		AnimNextContext.SetupForExecution(this, LatentHandles, DestinationBasePtr, bIsFrozen);

		VM->ExecuteVM(ExtendedExecuteContext, FRigUnit_AnimNextShimRoot::EventName);

		// Reset the context to avoid issues if we forget to reset it the next time we use it
		AnimNextContext.DebugReset();
	}
}

void FAnimNextGraphInstance::Freeze()
{
	if (!IsValid())
	{
		return;
	}

	GraphInstancePtr.Reset();
	ExtendedExecuteContext.Reset();
	Components.Empty();
}

void FAnimNextGraphInstance::Thaw()
{
	if (const UAnimNextGraph* GraphPtr = Graph)
	{
		ExtendedExecuteContext.CopyMemoryStorage(GraphPtr->ExtendedExecuteContext);
		GraphPtr->VM->InitializeInstance(ExtendedExecuteContext);

		{
			UE::AnimNext::FExecutionContext Context(*this);
			if(const FAnimNextDecoratorHandle* FoundHandle = GraphPtr->ResolvedRootDecoratorHandles.Find(EntryPoint))
			{
				GraphInstancePtr = Context.AllocateNodeInstance(UE::AnimNext::FWeakDecoratorPtr(), *FoundHandle);
			}
		}

		if (!IsValid())
		{
			// We failed to allocate our instance, clear everything
			Release();
		}
	}
}
