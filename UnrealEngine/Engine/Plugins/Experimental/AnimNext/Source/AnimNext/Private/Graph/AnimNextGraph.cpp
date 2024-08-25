// Copyright Epic Games, Inc. All Rights Reserved.

#include "Graph/AnimNextGraph.h"
#include "RigVMCore/RigVMMemoryStorage.h"
#include "RigVMCore/RigVMExecuteContext.h"
#include "Graph/RigUnit_AnimNextShimRoot.h"
#include "UObject/AssetRegistryTagsContext.h"
#include "UObject/ObjectSaveContext.h"
#include "UObject/Package.h"
#include "Param/ParamTypeHandle.h"
#include "DecoratorBase/DecoratorReader.h"
#include "DecoratorBase/ExecutionContext.h"
#include "Graph/AnimNext_LODPose.h"
#include "Graph/GC_GraphInstanceComponent.h"
#include "Graph/AnimNextGraphInstance.h"
#include "Serialization/MemoryReader.h"
#include "AnimNextStats.h"
#include "Graph/AnimNextGraphEntryPoint.h"
#include "Graph/RigUnit_AnimNextGraphRoot.h"

DEFINE_STAT(STAT_AnimNext_Graph_AllocateInstance);

#include UE_INLINE_GENERATED_CPP_BY_NAME(AnimNextGraph)

const UE::AnimNext::FParamId UAnimNextGraph::DefaultReferencePoseId = UE::AnimNext::FParamId("UE_AnimNextMeshComponent_ReferencePose");
const UE::AnimNext::FParamId UAnimNextGraph::DefaultCurrentLODId = UE::AnimNext::FParamId("UE_AnimNextMeshComponent_PredictedLODLevel");

UAnimNextGraph::UAnimNextGraph(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ExtendedExecuteContext.SetContextPublicDataStruct(FAnimNextExecuteContext::StaticStruct());
}

void UAnimNextGraph::AllocateInstance(FAnimNextGraphInstancePtr& Instance, FName InEntryPoint) const
{
	AllocateInstanceImpl(nullptr, Instance, InEntryPoint);
}

void UAnimNextGraph::AllocateInstance(FAnimNextGraphInstance& ParentGraphInstance, FAnimNextGraphInstancePtr& Instance, FName InEntryPoint) const
{
	AllocateInstanceImpl(&ParentGraphInstance, Instance, InEntryPoint);
}

void UAnimNextGraph::AllocateInstanceImpl(FAnimNextGraphInstance* ParentGraphInstance, FAnimNextGraphInstancePtr& Instance, FName InEntryPoint) const
{
	SCOPE_CYCLE_COUNTER(STAT_AnimNext_Graph_AllocateInstance);

	Instance.Release();

	const FName EntryPoint = (InEntryPoint == NAME_None) ? DefaultEntryPoint : InEntryPoint;
	const FAnimNextDecoratorHandle ResolvedRootDecoratorHandle = ResolvedRootDecoratorHandles.FindRef(EntryPoint);
	if (!ResolvedRootDecoratorHandle.IsValid())
	{
		return;
	}

	{
		TUniquePtr<FAnimNextGraphInstance> InstanceImpl = MakeUnique<FAnimNextGraphInstance>();

		InstanceImpl->Graph = this;
		InstanceImpl->ParentGraphInstance = ParentGraphInstance;
		InstanceImpl->EntryPoint = EntryPoint;

		// If we have a parent graph, use its root since we share the same root, otherwise if we have no parent, we are the root
		InstanceImpl->RootGraphInstance = ParentGraphInstance != nullptr ? ParentGraphInstance->GetRootGraphInstance() : InstanceImpl.Get();

		InstanceImpl->ExtendedExecuteContext.CopyMemoryStorage(ExtendedExecuteContext);
		VM->InitializeInstance(InstanceImpl->ExtendedExecuteContext);

		// Move our implementation so that we can use the instance to allocate our root node
		Instance.Impl = MoveTemp(InstanceImpl);
	}

	{
		UE::AnimNext::FExecutionContext Context(Instance);
		Instance.Impl->GraphInstancePtr = Context.AllocateNodeInstance(UE::AnimNext::FWeakDecoratorPtr(), ResolvedRootDecoratorHandle);
	}

	if (!Instance.IsValid())
	{
		// We failed to allocate our instance, clear everything
		Instance.Release();
	}

#if WITH_EDITORONLY_DATA
	if (Instance.IsValid())
	{
		FScopeLock Lock(&GraphInstancesLock);
		check(!GraphInstances.Contains(Instance.Impl.Get()));
		GraphInstances.Add(Instance.Impl.Get());
	}
#endif
}

void UAnimNextGraph::PostLoad()
{
	using namespace UE::AnimNext;

	Super::PostLoad();

	ReferencePoseId = FParamId(ReferencePose);
	CurrentLODId = FParamId(CurrentLOD);
}

void UAnimNextGraph::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (Ar.IsLoading())
	{
		int32 SharedDataArchiveBufferSize = 0;
		Ar << SharedDataArchiveBufferSize;

#if !WITH_EDITORONLY_DATA
		// When editor data isn't present, we don't persist the archive buffer as it is only needed on load
		// to populate the graph shared data
		TArray<uint8> SharedDataArchiveBuffer;
#endif

		SharedDataArchiveBuffer.SetNumUninitialized(SharedDataArchiveBufferSize);
		Ar.Serialize(SharedDataArchiveBuffer.GetData(), SharedDataArchiveBufferSize);

		if (Ar.IsLoadingFromCookedPackage())
		{
			// If we are cooked, we populate our graph shared data otherwise in the editor we'll compile on load
			// and re-populate everything then to account for changes in code/content
			LoadFromArchiveBuffer(SharedDataArchiveBuffer);
		}
	}
	else if (Ar.IsSaving())
	{
#if WITH_EDITORONLY_DATA
		// We only save the archive buffer, if code changes we'll be able to de-serialize from it when
		// building the runtime buffer
		// This allows us to have editor only/non-shipping only properties that are stripped out on load
		int32 SharedDataArchiveBufferSize = SharedDataArchiveBuffer.Num();
		Ar << SharedDataArchiveBufferSize;
		Ar.Serialize(SharedDataArchiveBuffer.GetData(), SharedDataArchiveBufferSize);
#endif
	}
	else
	{
		// Counting, etc
		Ar << SharedDataBuffer;

#if WITH_EDITORONLY_DATA
		Ar << SharedDataArchiveBuffer;
#endif
	}
}

TConstArrayView<UE::AnimNext::FScheduleTerm> UAnimNextGraph::GetTerms() const
{
	using namespace UE::AnimNext;

	static const FParamId OutputPoseId("UE_Internal_Graph_OutputPose");

	static const FScheduleTerm Terms[] =
	{
		FScheduleTerm(OutputPoseId, FAnimNextParamType::GetType<FAnimNextGraphLODPose>(), EScheduleTermDirection::Output)
	};

	return Terms;
}

bool UAnimNextGraph::LoadFromArchiveBuffer(const TArray<uint8>& InSharedDataArchiveBuffer)
{
	using namespace UE::AnimNext;

	// Reconstruct our graph shared data
	FMemoryReader GraphSharedDataArchive(InSharedDataArchiveBuffer);
	FDecoratorReader DecoratorReader(GraphReferencedObjects, GraphSharedDataArchive);

	const FDecoratorReader::EErrorState ErrorState = DecoratorReader.ReadGraph(SharedDataBuffer);
	if (ErrorState == FDecoratorReader::EErrorState::None)
	{
		for(FAnimNextGraphEntryPoint& EntryPoint : EntryPoints)
		{
			ResolvedRootDecoratorHandles.Add(EntryPoint.EntryPointName, DecoratorReader.ResolveEntryPointHandle(EntryPoint.RootDecoratorHandle));
		}

		// Make sure our execute method is registered
		FRigUnit_AnimNextGraphEvaluator::RegisterExecuteMethod(ExecuteDefinition);
		return true;
	}
	else
	{
		SharedDataBuffer.Empty(0);
		ResolvedRootDecoratorHandles.Add(FRigUnit_AnimNextGraphRoot::DefaultEntryPoint, FAnimNextDecoratorHandle());
		return false;
	}
}

#if WITH_EDITORONLY_DATA
void UAnimNextGraph::FreezeGraphInstances()
{
	FScopeLock Lock(&GraphInstancesLock);

	TSet<FAnimNextGraphInstance*> GraphInstancesCopy = GraphInstances;
	for (FAnimNextGraphInstance* GraphInstance : GraphInstancesCopy)
	{
		GraphInstance->Freeze();
	}
}

void UAnimNextGraph::ThawGraphInstances()
{
	FScopeLock Lock(&GraphInstancesLock);

	TSet<FAnimNextGraphInstance*> GraphInstancesCopy = GraphInstances;
	for (FAnimNextGraphInstance* GraphInstance : GraphInstancesCopy)
	{
		GraphInstance->Thaw();
	}
}
#endif
