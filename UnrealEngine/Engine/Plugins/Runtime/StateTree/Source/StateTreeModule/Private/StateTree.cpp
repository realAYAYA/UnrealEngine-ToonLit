// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTree.h"
#include "StateTreeLinker.h"
#include "StateTreeNodeBase.h"
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/ScopeRWLock.h"
#include "StateTreeDelegates.h"
#include "Logging/LogScopedVerbosityOverride.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTree)

const FGuid FStateTreeCustomVersion::GUID(0x28E21331, 0x501F4723, 0x8110FA64, 0xEA10DA1E);
FCustomVersionRegistration GRegisterStateTreeCustomVersion(FStateTreeCustomVersion::GUID, FStateTreeCustomVersion::LatestVersion, TEXT("StateTreeAsset"));

bool UStateTree::IsReadyToRun() const
{
	// Valid tree must have at least one state and valid instance data.
	return States.Num() > 0 && bIsLinked;
}

TSharedPtr<FStateTreeInstanceData> UStateTree::GetSharedInstanceData() const
{
	// Create a unique index for each thread.
	static std::atomic_int ThreadIndexCounter {0};
	static thread_local int32 ThreadIndex = INDEX_NONE; // Cannot init directly on WinRT
	if (ThreadIndex == INDEX_NONE)
	{
		ThreadIndex = ThreadIndexCounter.fetch_add(1);
	}

	// If shared instance data for this thread exists, return it.
	{
		FReadScopeLock ReadLock(PerThreadSharedInstanceDataLock);
		if (ThreadIndex < PerThreadSharedInstanceData.Num())
		{
			return PerThreadSharedInstanceData[ThreadIndex];
		}
	}

	// Not initialized yet, create new instances up to the index.
	FWriteScopeLock WriteLock(PerThreadSharedInstanceDataLock);

	// It is possible that multiple threads are waiting for the write lock,
	// which means that execution may get here so that 'ThreadIndex' is already in valid range.
	// The loop below is organized to handle that too.
	
	const int32 NewNum = ThreadIndex + 1;
	PerThreadSharedInstanceData.Reserve(NewNum);
	UStateTree* NonConstThis = const_cast<UStateTree*>(this); 
	
	for (int32 Index = PerThreadSharedInstanceData.Num(); Index < NewNum; Index++)
	{
		TSharedPtr<FStateTreeInstanceData> SharedData = MakeShared<FStateTreeInstanceData>();
		SharedData->CopyFrom(*NonConstThis, SharedInstanceData);
		PerThreadSharedInstanceData.Add(SharedData);
	}

	return PerThreadSharedInstanceData[ThreadIndex];
}

#if WITH_EDITOR
void UStateTree::ResetCompiled()
{
	Schema = nullptr;
	States.Reset();
	Transitions.Reset();
	Nodes.Reset();
	DefaultInstanceData.Reset();
	SharedInstanceData.Reset();
	ContextDataDescs.Reset();
	PropertyBindings.Reset();
	Parameters.Reset();

	ParametersDataViewIndex = FStateTreeIndex8::Invalid;
	
	EvaluatorsBegin = 0;
	EvaluatorsNum = 0;

	ResetLinked();
}

void UStateTree::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	const FString SchemaClassName = Schema ? Schema->GetClass()->GetPathName() : TEXT("");
	OutTags.Add(FAssetRegistryTag(UE::StateTree::SchemaTag, SchemaClassName, FAssetRegistryTag::TT_Alphabetical));

	Super::GetAssetRegistryTags(OutTags);
}

void UStateTree::PostLoadAssetRegistryTags(const FAssetData& InAssetData, TArray<FAssetRegistryTag>& OutTagsAndValuesToUpdate) const
{
	Super::PostLoadAssetRegistryTags(InAssetData, OutTagsAndValuesToUpdate);

	static const FName SchemaTag(TEXT("Schema"));
	FString SchemaTagValue = InAssetData.GetTagValueRef<FString>(SchemaTag);
	if (!SchemaTagValue.IsEmpty() && FPackageName::IsShortPackageName(SchemaTagValue))
	{
		FTopLevelAssetPath SchemaTagClassPathName = UClass::TryConvertShortTypeNameToPathName<UStruct>(SchemaTagValue, ELogVerbosity::Warning, TEXT("UStateTree::PostLoadAssetRegistryTags"));
		if (!SchemaTagClassPathName.IsNull())
		{
			OutTagsAndValuesToUpdate.Add(FAssetRegistryTag(SchemaTag, SchemaTagClassPathName.ToString(), FAssetRegistryTag::TT_Alphabetical));
		}
	}
}

#endif // WITH_EDITOR

void UStateTree::AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector)
{
	Super::AddReferencedObjects(InThis, Collector);

	const UStateTree* StateTree = Cast<UStateTree>(InThis);
	check(StateTree);
	
	FReadScopeLock ReadLock(StateTree->PerThreadSharedInstanceDataLock);

	for (const TSharedPtr<FStateTreeInstanceData>& InstanceData : StateTree->PerThreadSharedInstanceData)
	{
		if (InstanceData.IsValid())
		{
			uint8* StructMemory = (uint8*)InstanceData.Get();
			const UScriptStruct* ScriptStruct = FStateTreeInstanceData::StaticStruct();
			Collector.AddReferencedObjects(ScriptStruct, StructMemory);
		}
	}
}

void UStateTree::PostLoad()
{
	Super::PostLoad();

	const int32 CurrentVersion = GetLinkerCustomVersion(FStateTreeCustomVersion::GUID);

	if (CurrentVersion < FStateTreeCustomVersion::LatestVersion)
	{
#if WITH_EDITOR
		// Compiled data is in older format, try to compile the StateTree.
		if (UE::StateTree::Delegates::OnRequestCompile.IsBound())
		{
			LOG_SCOPE_VERBOSITY_OVERRIDE(LogStateTree, ELogVerbosity::Log);
			UE_LOG(LogStateTree, Log, TEXT("%s: compiled data is in older format. Trying to compile the asset..."), *GetFullName());
			UE::StateTree::Delegates::OnRequestCompile.Execute(*this);
		}
		else
		{
			ResetCompiled();
			UE_LOG(LogStateTree, Warning, TEXT("%s: compiled data is in older format. Please resave the StateTree asset."), *GetFullName());
		}
#else
		UE_LOG(LogStateTree, Error, TEXT("%s: compiled data is in older format. Please recompile the StateTree asset."), *GetFullName());
#endif
		return;
	}
	
	if (!Link())
	{
		UE_LOG(LogStateTree, Error, TEXT("%s failed to link. Asset will not be usable at runtime."), *GetFullName());	
	}
}

void UStateTree::Serialize(FStructuredArchiveRecord Record)
{
	Super::Serialize(Record);

	Record.GetUnderlyingArchive().UsingCustomVersion(FStateTreeCustomVersion::GUID);
	
	// We need to link and rebind property bindings each time a BP is compiled,
	// because property bindings may get invalid, and instance data potentially needs refreshed.
	if (Record.GetUnderlyingArchive().IsModifyingWeakAndStrongReferences())
	{
		if (!Link())
		{
			UE_LOG(LogStateTree, Error, TEXT("%s failed to link. Asset will not be usable at runtime."), *GetName());	
		}
	}
}

void UStateTree::ResetLinked()
{
	bIsLinked = false;
	ExternalDataDescs.Reset();
	ExternalDataBaseIndex = 0;
	NumDataViews = 0;

	FWriteScopeLock WriteLock(PerThreadSharedInstanceDataLock);
	PerThreadSharedInstanceData.Reset();
}

bool UStateTree::Link()
{
	// Initialize the instance data default value.
	// This data will be used to allocate runtime instance on all StateTree users.
	ResetLinked();

	if (States.Num() > 0 && Nodes.Num() > 0)
	{
		if (!DefaultInstanceData.IsValid())
		{
			UE_LOG(LogStateTree, Error, TEXT("%s: StartTree does not have instance data. Please recompile the StateTree asset."), *GetName());
			return false;
		}
		
		// Update property bag structs before resolving binding.
		const TArrayView<FStateTreeBindableStructDesc> SourceStructs = PropertyBindings.GetSourceStructs();
		const TArrayView<FStateTreePropCopyBatch> CopyBatches = PropertyBindings.GetCopyBatches();

		if (ParametersDataViewIndex.IsValid() && SourceStructs.IsValidIndex(ParametersDataViewIndex.Get()))
		{
			SourceStructs[ParametersDataViewIndex.Get()].Struct = Parameters.GetPropertyBagStruct();
		}
		
		for (const FCompactStateTreeState& State : States)
		{
			if (State.Type == EStateTreeStateType::Subtree)
			{
				if (State.ParameterInstanceIndex.IsValid() == false)
				{
					UE_LOG(LogStateTree, Error, TEXT("%s: Data for state '%s' is malformed. Please recompile the StateTree asset."), *GetName(), *State.Name.ToString());
					return false;
				}

				// Subtree is a bind source, update bag struct.
				if (State.ParameterDataViewIndex.IsValid())
				{
					const FCompactStateTreeParameters& Params = DefaultInstanceData.GetMutableStruct(State.ParameterInstanceIndex.Get()).GetMutable<FCompactStateTreeParameters>();
					FStateTreeBindableStructDesc& Desc = SourceStructs[State.ParameterDataViewIndex.Get()];
					Desc.Struct = Params.Parameters.GetPropertyBagStruct();
				}
			}
			else if (State.Type == EStateTreeStateType::Linked && State.LinkedState.IsValid())
			{
				const FCompactStateTreeState& LinkedState = States[State.LinkedState.Index];

				if (State.ParameterInstanceIndex.IsValid() == false
					|| LinkedState.ParameterInstanceIndex.IsValid() == false)
				{
					UE_LOG(LogStateTree, Error, TEXT("%s: Data for state '%s' is malformed. Please recompile the StateTree asset."), *GetName(), *State.Name.ToString());
					return false;
				}

				const FCompactStateTreeParameters& Params = DefaultInstanceData.GetMutableStruct(State.ParameterInstanceIndex.Get()).GetMutable<FCompactStateTreeParameters>();

				// Check that the bag in linked state matches.
				const FCompactStateTreeParameters& LinkedStateParams = DefaultInstanceData.GetMutableStruct(LinkedState.ParameterInstanceIndex.Get()).GetMutable<FCompactStateTreeParameters>();

				if (LinkedStateParams.Parameters.GetPropertyBagStruct() != Params.Parameters.GetPropertyBagStruct())
				{
					UE_LOG(LogStateTree, Error, TEXT("%s: The parameters on state '%s' does not match the linked state parameters in state '%s'. Please recompile the StateTree asset."), *GetName(), *State.Name.ToString(), *LinkedState.Name.ToString());
					return false;
				}

				if (Params.BindingsBatch.IsValid())
				{
					FStateTreePropCopyBatch& Batch = CopyBatches[Params.BindingsBatch.Get()];
					Batch.TargetStruct.Struct = Params.Parameters.GetPropertyBagStruct();
				}
			}
		}
		
		// Resolves property paths used by bindings a store property pointers
		if (!PropertyBindings.ResolvePaths())
		{
			return false;
		}
	}

	// Resolves nodes references to other StateTree data
	FStateTreeLinker Linker(Schema);
	Linker.SetExternalDataBaseIndex(PropertyBindings.GetSourceStructNum());
	
	for (int32 Index = 0; Index < Nodes.Num(); Index++)
	{
		const FStructView Node = Nodes[Index];
		if (FStateTreeNodeBase* NodePtr = Node.GetMutablePtr<FStateTreeNodeBase>())
		{
			Linker.SetCurrentInstanceDataType(NodePtr->GetInstanceDataType(), NodePtr->DataViewIndex.Get());
			const bool bLinkSucceeded = NodePtr->Link(Linker);
			if (!bLinkSucceeded || Linker.GetStatus() == EStateTreeLinkerStatus::Failed)
			{
				UE_LOG(LogStateTree, Error, TEXT("%s: node '%s' failed to resolve its references."), *GetName(), *NodePtr->StaticStruct()->GetName());
				return false;
			}
		}
	}

	// Link succeeded, setup tree to be ready to run
	ExternalDataBaseIndex = PropertyBindings.GetSourceStructNum();
	ExternalDataDescs = Linker.GetExternalDataDescs();
	NumDataViews = ExternalDataBaseIndex + ExternalDataDescs.Num();

	bIsLinked = true;
	
	return true;
}

#if WITH_EDITOR

void FStateTreeMemoryUsage::AddUsage(FConstStructView View)
{
	if (const UScriptStruct* ScriptStruct = View.GetScriptStruct())
	{
		EstimatedMemoryUsage = Align(EstimatedMemoryUsage, ScriptStruct->GetMinAlignment());
		EstimatedMemoryUsage += ScriptStruct->GetStructureSize();
	}
}

void FStateTreeMemoryUsage::AddUsage(const UObject* Object)
{
	if (Object != nullptr)
	{
		check(Object->GetClass());
		EstimatedMemoryUsage += Object->GetClass()->GetStructureSize();
	}
}

TArray<FStateTreeMemoryUsage> UStateTree::CalculateEstimatedMemoryUsage() const
{
	TArray<FStateTreeMemoryUsage> MemoryUsages;
	TArray<TPair<int32, int32>> StateLinks;

	if (States.IsEmpty() || !Nodes.IsValid() || !DefaultInstanceData.IsValid())
	{
		return MemoryUsages;
	}

	const int32 TreeMemUsageIndex = MemoryUsages.Emplace(TEXT("State Tree Max"));
	const int32 InstanceMemUsageIndex = MemoryUsages.Emplace(TEXT("Instance Overhead"));
	const int32 EvalMemUsageIndex = MemoryUsages.Emplace(TEXT("Evaluators"));
	const int32 SharedMemUsageIndex = MemoryUsages.Emplace(TEXT("Shared Data"));

	auto GetRootStateHandle = [this](const FStateTreeStateHandle InState) -> FStateTreeStateHandle
	{
		FStateTreeStateHandle Result = InState;
		while (Result.IsValid() && States[Result.Index].Parent.IsValid())
		{
			Result = States[Result.Index].Parent;
		}
		return Result;		
	};

	auto GetUsageIndexForState = [&MemoryUsages, this](const FStateTreeStateHandle InStateHandle) -> int32
	{
		check(InStateHandle.IsValid());
		
		const int32 FoundMemUsage = MemoryUsages.IndexOfByPredicate([InStateHandle](const FStateTreeMemoryUsage& MemUsage) { return MemUsage.Handle == InStateHandle; });
		if (FoundMemUsage != INDEX_NONE)
		{
			return FoundMemUsage;
		}

		const FCompactStateTreeState& CompactState = States[InStateHandle.Index];
		
		return MemoryUsages.Emplace(TEXT("State ") + CompactState.Name.ToString(), InStateHandle);
	};

	// Calculate memory usage per state.
	TArray<FStateTreeMemoryUsage> TempStateMemoryUsages;
	TempStateMemoryUsages.SetNum(States.Num());

	for (int32 Index = 0; Index < States.Num(); Index++)
	{
		const FStateTreeStateHandle StateHandle((uint16)Index);
		const FCompactStateTreeState& CompactState = States[Index];
		const FStateTreeStateHandle ParentHandle = GetRootStateHandle(StateHandle);
		const int32 ParentUsageIndex = GetUsageIndexForState(ParentHandle);
		
		FStateTreeMemoryUsage& MemUsage = CompactState.Parent.IsValid() ? TempStateMemoryUsages[Index] : MemoryUsages[GetUsageIndexForState(StateHandle)];
		
		MemUsage.NodeCount += CompactState.TasksNum;

		if (CompactState.Type == EStateTreeStateType::Linked)
		{
			const int32 LinkedUsageIndex = GetUsageIndexForState(CompactState.LinkedState);
			StateLinks.Emplace(ParentUsageIndex, LinkedUsageIndex);

			MemUsage.NodeCount++;
			MemUsage.AddUsage(DefaultInstanceData.GetStruct(CompactState.ParameterInstanceIndex.Get()));
		}
		
		for (int32 TaskIndex = CompactState.TasksBegin; TaskIndex < (CompactState.TasksBegin + CompactState.TasksNum); TaskIndex++)
		{
			const FStateTreeTaskBase& Task = Nodes[TaskIndex].Get<FStateTreeTaskBase>();
			if (Task.bInstanceIsObject == false)
			{
				MemUsage.NodeCount++;
				MemUsage.AddUsage(DefaultInstanceData.GetStruct(Task.InstanceIndex.Get()));
			}
			else
			{
				MemUsage.NodeCount++;
				MemUsage.AddUsage(DefaultInstanceData.GetMutableObject(Task.InstanceIndex.Get()));
			}
		}
	}

	// Combine max child usage to parents. Iterate backwards to update children first.
	for (int32 Index = States.Num() - 1; Index >= 0; Index--)
	{
		const FStateTreeStateHandle StateHandle((uint16)Index);
		const FCompactStateTreeState& CompactState = States[Index];

		FStateTreeMemoryUsage& MemUsage = CompactState.Parent.IsValid() ? TempStateMemoryUsages[Index] : MemoryUsages[GetUsageIndexForState(StateHandle)];

		int32 MaxChildStateMem = 0;
		int32 MaxChildStateNodes = 0;
		
		for (uint16 ChildState = CompactState.ChildrenBegin; ChildState < CompactState.ChildrenEnd; ChildState = States[ChildState].GetNextSibling())
		{
			const FStateTreeMemoryUsage& ChildMemUsage = TempStateMemoryUsages[ChildState];
			if (ChildMemUsage.EstimatedMemoryUsage > MaxChildStateMem)
			{
				MaxChildStateMem = ChildMemUsage.EstimatedMemoryUsage;
				MaxChildStateNodes = ChildMemUsage.NodeCount;
			}
		}

		MemUsage.EstimatedMemoryUsage += MaxChildStateMem;
		MemUsage.NodeCount += MaxChildStateNodes;
	}

	// Accumulate linked states.
	for (int32 Index = StateLinks.Num() - 1; Index >= 0; Index--)
	{
		FStateTreeMemoryUsage& ParentUsage = MemoryUsages[StateLinks[Index].Get<0>()];
		const FStateTreeMemoryUsage& LinkedUsage = MemoryUsages[StateLinks[Index].Get<1>()];
		const int32 LinkedTotalUsage = LinkedUsage.EstimatedMemoryUsage + LinkedUsage.EstimatedChildMemoryUsage;
		if (LinkedTotalUsage > ParentUsage.EstimatedChildMemoryUsage)
		{
			ParentUsage.EstimatedChildMemoryUsage = LinkedTotalUsage;
			ParentUsage.ChildNodeCount = LinkedUsage.NodeCount + LinkedUsage.ChildNodeCount;
		}
	}

	// Evaluators
	FStateTreeMemoryUsage& EvalMemUsage = MemoryUsages[EvalMemUsageIndex];
	for (int32 EvalIndex = EvaluatorsBegin; EvalIndex < (EvaluatorsBegin + EvaluatorsNum); EvalIndex++)
	{
		const FStateTreeEvaluatorBase& Eval = Nodes[EvalIndex].Get<FStateTreeEvaluatorBase>();
		if (Eval.bInstanceIsObject == false)
		{
			EvalMemUsage.AddUsage(DefaultInstanceData.GetMutableStruct(Eval.InstanceIndex.Get()));
		}
		else
		{
			EvalMemUsage.AddUsage(DefaultInstanceData.GetMutableObject(Eval.InstanceIndex.Get()));
		}
		EvalMemUsage.NodeCount++;
	}

	// Estimate highest combined usage.
	FStateTreeMemoryUsage& TreeMemUsage = MemoryUsages[TreeMemUsageIndex];

	// Exec state
	TreeMemUsage.AddUsage(DefaultInstanceData.GetStruct(0));
	TreeMemUsage.NodeCount++;

	TreeMemUsage.EstimatedMemoryUsage += EvalMemUsage.EstimatedMemoryUsage;
	TreeMemUsage.NodeCount += EvalMemUsage.NodeCount;

	FStateTreeMemoryUsage& InstanceMemUsage = MemoryUsages[InstanceMemUsageIndex];
	// FStateTreeInstanceData overhead.
	InstanceMemUsage.EstimatedMemoryUsage += sizeof(FStateTreeInstanceData);
	// FInstancedStructArray overhead.
	constexpr int32 ItemSize = 16; // sizeof(FInstancedStructArray::FItem);
	InstanceMemUsage.EstimatedMemoryUsage += TreeMemUsage.NodeCount * ItemSize;

	TreeMemUsage.EstimatedMemoryUsage += InstanceMemUsage.EstimatedMemoryUsage;
	
	int32 MaxSubtreeUsage = 0;
	int32 MaxSubtreeNodeCount = 0;
	
	for (const FStateTreeMemoryUsage& MemUsage : MemoryUsages)
	{
		if (MemUsage.Handle.IsValid())
		{
			const int32 TotalUsage = MemUsage.EstimatedMemoryUsage + MemUsage.EstimatedChildMemoryUsage;
			if (TotalUsage > MaxSubtreeUsage)
			{
				MaxSubtreeUsage = TotalUsage;
				MaxSubtreeNodeCount = MemUsage.NodeCount + MemUsage.ChildNodeCount;
			}
		}
	}

	TreeMemUsage.EstimatedMemoryUsage += MaxSubtreeUsage;
	TreeMemUsage.NodeCount += MaxSubtreeNodeCount;

	if (SharedInstanceData.IsValid())
	{
		FStateTreeMemoryUsage& SharedMemUsage = MemoryUsages[SharedMemUsageIndex];
		SharedMemUsage.NodeCount = SharedInstanceData.GetNumItems();
		SharedMemUsage.EstimatedMemoryUsage = SharedInstanceData.GetEstimatedMemoryUsage();
	}

	return MemoryUsages;
}
#endif // WITH_EDITOR

