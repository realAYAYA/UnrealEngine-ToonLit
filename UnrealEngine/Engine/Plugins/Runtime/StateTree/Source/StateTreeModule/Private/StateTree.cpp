// Copyright Epic Games, Inc. All Rights Reserved.

#include "StateTree.h"
#include "Misc/PackageName.h"
#include "StateTreeLinker.h"
#include "StateTreeTaskBase.h"
#include "StateTreeEvaluatorBase.h"
#include "StateTreeConditionBase.h"
#include "AssetRegistry/AssetData.h"
#include "Misc/ScopeRWLock.h"
#include "StateTreeDelegates.h"
#include "Logging/LogScopedVerbosityOverride.h"
#include "Misc/DataValidation.h"
#include "StructUtilsDelegates.h"
#if WITH_EDITOR
#include "Engine/UserDefinedStruct.h"
#endif
#include UE_INLINE_GENERATED_CPP_BY_NAME(StateTree)

const FGuid FStateTreeCustomVersion::GUID(0x28E21331, 0x501F4723, 0x8110FA64, 0xEA10DA1E);
FCustomVersionRegistration GRegisterStateTreeCustomVersion(FStateTreeCustomVersion::GUID, FStateTreeCustomVersion::LatestVersion, TEXT("StateTreeAsset"));

bool UStateTree::IsReadyToRun() const
{
	// Valid tree must have at least one state and valid instance data.
	return States.Num() > 0 && bIsLinked;
}

FConstStructView UStateTree::GetNode(const int32 NodeIndex) const
{
	return Nodes.IsValidIndex(NodeIndex) ? Nodes[NodeIndex] : FConstStructView();	
}

FStateTreeIndex16 UStateTree::GetNodeIndexFromId(const FGuid Id) const
{
	const FStateTreeNodeIdToIndex* Entry = IDToNodeMappings.FindByPredicate([Id](const FStateTreeNodeIdToIndex& Entry){ return Entry.Id == Id; });
	return Entry != nullptr ? Entry->Index : FStateTreeIndex16::Invalid;
}

FGuid UStateTree::GetNodeIdFromIndex(const FStateTreeIndex16 NodeIndex) const
{
	const FStateTreeNodeIdToIndex* Entry = NodeIndex.IsValid()
		? IDToNodeMappings.FindByPredicate([NodeIndex](const FStateTreeNodeIdToIndex& Entry){ return Entry.Index == NodeIndex; })
		: nullptr;
	return Entry != nullptr ? Entry->Id : FGuid();
}

const FCompactStateTreeState* UStateTree::GetStateFromHandle(const FStateTreeStateHandle StateHandle) const
{
	return States.IsValidIndex(StateHandle.Index) ? &States[StateHandle.Index] : nullptr;
}

FStateTreeStateHandle UStateTree::GetStateHandleFromId(const FGuid Id) const
{
	const FStateTreeStateIdToHandle* Entry = IDToStateMappings.FindByPredicate([Id](const FStateTreeStateIdToHandle& Entry){ return Entry.Id == Id; });
	return Entry != nullptr ? Entry->Handle : FStateTreeStateHandle::Invalid;
}

FGuid UStateTree::GetStateIdFromHandle(const FStateTreeStateHandle Handle) const
{
	const FStateTreeStateIdToHandle* Entry = IDToStateMappings.FindByPredicate([Handle](const FStateTreeStateIdToHandle& Entry){ return Entry.Handle == Handle; });
	return Entry != nullptr ? Entry->Id : FGuid();
}

const FCompactStateTransition* UStateTree::GetTransitionFromIndex(const FStateTreeIndex16 TransitionIndex) const
{
	return TransitionIndex.IsValid() && Transitions.IsValidIndex(TransitionIndex.Get()) ? &Transitions[TransitionIndex.Get()] : nullptr;
}

FStateTreeIndex16 UStateTree::GetTransitionIndexFromId(const FGuid Id) const
{
	const FStateTreeTransitionIdToIndex* Entry = IDToTransitionMappings.FindByPredicate([Id](const FStateTreeTransitionIdToIndex& Entry){ return Entry.Id == Id; });
	return Entry != nullptr ? Entry->Index : FStateTreeIndex16::Invalid;
}

FGuid UStateTree::GetTransitionIdFromIndex(const FStateTreeIndex16 Index) const
{
	const FStateTreeTransitionIdToIndex* Entry = IDToTransitionMappings.FindByPredicate([Index](const FStateTreeTransitionIdToIndex& Entry){ return Entry.Index == Index; });
	return Entry != nullptr ? Entry->Id : FGuid();
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
	IDToStateMappings.Reset();
	IDToNodeMappings.Reset();
	IDToTransitionMappings.Reset();

	ParametersDataViewIndex = FStateTreeIndex8::Invalid;
	
	EvaluatorsBegin = 0;
	EvaluatorsNum = 0;

	GlobalTasksBegin = 0;
	GlobalTasksNum = 0;
	bHasGlobalTransitionTasks = false;
	
	ResetLinked();
}

void UStateTree::OnObjectsReinstanced(const FReplacementObjectMap& ObjectMap)
{
	if (ObjectMap.IsEmpty())
	{
		return;
	}

	// If the asset is not linked yet (or has failed), no need to link.
	if (!bIsLinked)
	{
		return;
	}

	// Relink only if the reinstantiated object belongs to this asset,
	// or anything from the property binding refers to the classes of the reinstantiated object.

	bool bShouldRelink = false;

	for (TMap<UObject*, UObject*>::TConstIterator It(ObjectMap); It; ++It)
	{
		if (const UObject* ObjectToBeReplaced = It->Value)
		{
			if (ObjectToBeReplaced->IsInOuter(this))
			{
				bShouldRelink = true;
				break;
			}
		}			
	}

	if (!bShouldRelink)
	{
		TSet<const UStruct*> Structs;
		for (TMap<UObject*, UObject*>::TConstIterator It(ObjectMap); It; ++It)
		{
			if (const UObject* ObjectToBeReplaced = It->Value)
			{
				Structs.Add(ObjectToBeReplaced->GetClass());
			}
		}

		bShouldRelink |= PropertyBindings.ContainsAnyStruct(Structs);
	}

	if (bShouldRelink)
	{
		if (!Link())
		{
			UE_LOG(LogStateTree, Error, TEXT("%s failed to link after Object reinstantiation. Take a look at the asset for any errors. Asset will not be usable at runtime."), *GetFullName());
		}
	}
}

void UStateTree::OnUserDefinedStructReinstanced(const UUserDefinedStruct& UserDefinedStruct)
{
	// Struct utils handle reinstancing the struct values (instanced struct, property bag, etc).
	// We will need to update the property binding.
	
	TSet<const UStruct*> Structs;
	Structs.Add(&UserDefinedStruct);
	
	if (PropertyBindings.ContainsAnyStruct(Structs))
	{
		if (!Link())
		{
			UE_LOG(LogStateTree, Error, TEXT("%s failed to link after Struct reinstantiation. Take a look at the asset for any errors. Asset will not be usable at runtime."), *GetFullName());
		}
	}
}

void UStateTree::PostInitProperties()
{
	Super::PostInitProperties();
	
	OnObjectsReinstancedHandle = FCoreUObjectDelegates::OnObjectsReinstanced.AddUObject(this, &UStateTree::OnObjectsReinstanced);
	OnUserDefinedStructReinstancedHandle = UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.AddUObject(this, &UStateTree::OnUserDefinedStructReinstanced);
}

void UStateTree::BeginDestroy()
{
	if (OnObjectsReinstancedHandle.IsValid())
	{
		FCoreUObjectDelegates::OnObjectsReinstanced.Remove(OnObjectsReinstancedHandle);
		OnObjectsReinstancedHandle.Reset();
	}
	if (OnUserDefinedStructReinstancedHandle.IsValid())
	{
		UE::StructUtils::Delegates::OnUserDefinedStructReinstanced.Remove(OnUserDefinedStructReinstancedHandle);
		OnUserDefinedStructReinstancedHandle.Reset();
	}
	
	Super::BeginDestroy();
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
	const FString SchemaTagValue = InAssetData.GetTagValueRef<FString>(SchemaTag);
	if (!SchemaTagValue.IsEmpty() && FPackageName::IsShortPackageName(SchemaTagValue))
	{
		const FTopLevelAssetPath SchemaTagClassPathName = UClass::TryConvertShortTypeNameToPathName<UStruct>(SchemaTagValue, ELogVerbosity::Warning, TEXT("UStateTree::PostLoadAssetRegistryTags"));
		if (!SchemaTagClassPathName.IsNull())
		{
			OutTagsAndValuesToUpdate.Add(FAssetRegistryTag(SchemaTag, SchemaTagClassPathName.ToString(), FAssetRegistryTag::TT_Alphabetical));
		}
	}
}

EDataValidationResult UStateTree::IsDataValid(FDataValidationContext& Context) const
{
	if (!const_cast<UStateTree*>(this)->Link())
	{
		Context.AddError(FText::FromString(FString::Printf(TEXT("%s failed to link. Please recompile the State Tree for more details errors."), *GetFullName())));
		return EDataValidationResult::Invalid;
	}

	return Super::IsDataValid(Context);
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
			Collector.AddPropertyReferencesWithStructARO(FStateTreeInstanceData::StaticStruct(), InstanceData.Get(), StateTree);
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
		UE_LOG(LogStateTree, Log, TEXT("%s failed to link. Asset will not be usable at runtime."), *GetFullName());	
	}
}

#if WITH_EDITORONLY_DATA
void UStateTree::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	TArray<UClass*> SchemaClasses;
	GetDerivedClasses(UStateTreeSchema::StaticClass(), SchemaClasses);
	for (UClass* SchemaClass : SchemaClasses)
	{
		if (!SchemaClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Transient))
		{
			OutConstructClasses.Add(FTopLevelAssetPath(SchemaClass));
		}
	}
}
#endif

void UStateTree::Serialize(const FStructuredArchiveRecord Record)
{
	Super::Serialize(Record);

	Record.GetUnderlyingArchive().UsingCustomVersion(FStateTreeCustomVersion::GUID);
	
	// We need to link and rebind property bindings each time a BP is compiled,
	// because property bindings may get invalid, and instance data potentially needs refreshed.
	if (Record.GetUnderlyingArchive().IsModifyingWeakAndStrongReferences())
	{
		if (!Link())
		{
			UE_LOG(LogStateTree, Log, TEXT("%s failed to link. Asset will not be usable at runtime."), *GetName());	
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

	// Resolves nodes references to other StateTree data
	FStateTreeLinker Linker(Schema);
	Linker.SetExternalDataBaseIndex(PropertyBindings.GetSourceStructNum());

	for (int32 Index = 0; Index < Nodes.Num(); Index++)
	{
		FStructView Node = Nodes[Index];
		if (FStateTreeNodeBase* NodePtr = Node.GetPtr<FStateTreeNodeBase>())
		{
			Linker.SetCurrentInstanceDataType(NodePtr->GetInstanceDataType(), NodePtr->DataViewIndex.Get());
			const bool bLinkSucceeded = NodePtr->Link(Linker);
			if (!bLinkSucceeded || Linker.GetStatus() == EStateTreeLinkerStatus::Failed)
			{
				UE_LOG(LogStateTree, Error, TEXT("%s: node '%s' failed to resolve its references."), *GetFullName(), *NodePtr->StaticStruct()->GetName());
				return false;
			}
		}
	}

	ExternalDataBaseIndex = PropertyBindings.GetSourceStructNum();
	ExternalDataDescs = Linker.GetExternalDataDescs();
	NumDataViews = ExternalDataBaseIndex + ExternalDataDescs.Num();

	if (States.Num() > 0 && Nodes.Num() > 0)
	{
		if (!DefaultInstanceData.IsValid())
		{
			UE_LOG(LogStateTree, Error, TEXT("%s: StartTree does not have instance data. Please recompile the StateTree asset."), *GetFullName());
			return false;
		}

		// Check that all nodes are valid.
		for (FConstStructView Node : Nodes)
		{
			if (!Node.IsValid())
			{
				UE_LOG(LogStateTree, Error, TEXT("%s: State Tree asset was not properly loaded (missing node). See log for loading failures, or recompile the StateTree asset."), *GetFullName());
				return false;
			}
		}

		if (!DefaultInstanceData.AreAllInstancesValid())
		{
			UE_LOG(LogStateTree, Error, TEXT("%s: State Tree asset was not properly loaded (missing instance data). See log for loading failures, or recompile the StateTree asset."), *GetFullName());
			return false;
		}

		if (!SharedInstanceData.AreAllInstancesValid())
		{
			UE_LOG(LogStateTree, Error, TEXT("%s: State Tree asset was not properly loaded (missing shared instance data). See log for loading failures, or recompile the StateTree asset."), *GetFullName());
			return false;
		}

		if (!PatchBindings())
		{
			return false;
		}

		// Resolves property paths used by bindings a store property pointers
		if (!PropertyBindings.ResolvePaths())
		{
			return false;
		}
	}

	// Link succeeded, setup tree to be ready to run
	bIsLinked = true;
	
	return true;
}

bool UStateTree::PatchBindings()
{
	const TArrayView<FStateTreeBindableStructDesc> SourceStructs = PropertyBindings.SourceStructs;
	const TArrayView<FStateTreePropertyCopyBatch> CopyBatches = PropertyBindings.CopyBatches;
	const TArrayView<FStateTreePropertyPathBinding> PropertyPathBindings = PropertyBindings.PropertyPathBindings;

	// Reconcile out of date classes.
	for (FStateTreeBindableStructDesc& SourceStruct : SourceStructs)
	{
		if (const UClass* SourceClass = Cast<UClass>(SourceStruct.Struct))
		{
			if (SourceClass->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				SourceStruct.Struct = SourceClass->GetAuthoritativeClass();
			}
		}
	}
	for (FStateTreePropertyCopyBatch& CopyBatch : CopyBatches)
	{
		if (const UClass* TargetClass = Cast<UClass>(CopyBatch.TargetStruct.Struct))
		{
			if (TargetClass->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				CopyBatch.TargetStruct.Struct = TargetClass->GetAuthoritativeClass();
			}
		}
	}

	auto PatchPropertyPath = [](FStateTreePropertyPath& PropertyPath)
	{
		for (FStateTreePropertyPathSegment& Segment : PropertyPath.GetMutableSegments())
		{
			if (const UClass* InstanceStruct = Cast<UClass>(Segment.GetInstanceStruct()))
			{
				if (InstanceStruct->HasAnyClassFlags(CLASS_NewerVersionExists))
				{
					Segment.SetInstanceStruct(InstanceStruct->GetAuthoritativeClass());
				}
			}
		}
	};

	for (FStateTreePropertyPathBinding& PropertyPathBinding : PropertyPathBindings)
	{
		PatchPropertyPath(PropertyPathBinding.GetMutableSourcePath());
		PatchPropertyPath(PropertyPathBinding.GetMutableTargetPath());
	}

	// Update property bag structs before resolving binding.
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
				UE_LOG(LogStateTree, Error, TEXT("%s: Data for state '%s' is malformed. Please recompile the StateTree asset."), *GetFullName(), *State.Name.ToString());
				return false;
			}

			// Subtree is a bind source, update bag struct.
			if (State.ParameterDataViewIndex.IsValid())
			{
				const FCompactStateTreeParameters& Params = DefaultInstanceData.GetMutableStruct(State.ParameterInstanceIndex.Get()).Get<FCompactStateTreeParameters>();
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
				UE_LOG(LogStateTree, Error, TEXT("%s: Data for state '%s' is malformed. Please recompile the StateTree asset."), *GetFullName(), *State.Name.ToString());
				return false;
			}

			const FCompactStateTreeParameters& Params = DefaultInstanceData.GetMutableStruct(State.ParameterInstanceIndex.Get()).Get<FCompactStateTreeParameters>();

			// Check that the bag in linked state matches.
			const FCompactStateTreeParameters& LinkedStateParams = DefaultInstanceData.GetMutableStruct(LinkedState.ParameterInstanceIndex.Get()).Get<FCompactStateTreeParameters>();

			if (LinkedStateParams.Parameters.GetPropertyBagStruct() != Params.Parameters.GetPropertyBagStruct())
			{
				UE_LOG(LogStateTree, Error, TEXT("%s: The parameters on state '%s' does not match the linked state parameters in state '%s'. Please recompile the StateTree asset."), *GetFullName(), *State.Name.ToString(), *LinkedState.Name.ToString());
				return false;
			}

			if (Params.BindingsBatch.IsValid())
			{
				FStateTreePropertyCopyBatch& Batch = CopyBatches[Params.BindingsBatch.Get()];
				Batch.TargetStruct.Struct = Params.Parameters.GetPropertyBagStruct();
			}
		}
	}

	TArray<FStateTreeDataView> DataViews;
	DataViews.SetNum(NumDataViews);

	// Tree parameters
	DataViews[ParametersDataViewIndex.Get()] = Parameters.GetMutableValue();

	// Setup data views for context data. Since the external data is passed at runtime, we can only provide the type.
	for (const FStateTreeExternalDataDesc& DataDesc : ContextDataDescs)
	{
		DataViews[DataDesc.Handle.DataViewIndex.Get()] = FStateTreeDataView(DataDesc.Struct, nullptr);
	}
	
	// Setup data views for state parameters.
	for (FCompactStateTreeState& State : States)
	{
		// Subtree state parameters can be a binding source, but not a target.
		// Linked state parameters can only by a binding target, so not handled here.
		if (State.Type == EStateTreeStateType::Subtree)
		{
			if (State.ParameterDataViewIndex.IsValid())
			{
				FCompactStateTreeParameters& Params = DefaultInstanceData.GetMutableStruct(State.ParameterInstanceIndex.Get()).Get<FCompactStateTreeParameters>();
				ensureMsgf(Params.Parameters.IsValid(), TEXT("Expecting valid state parameter value when parameter view is set."));
				DataViews[State.ParameterDataViewIndex.Get()] = Params.Parameters.GetMutableValue();
			}
		}
	}

	// Setup data views for all nodes.
	for (FConstStructView NodeView : Nodes)
	{
		if (const FStateTreeConditionBase* Condition = NodeView.GetPtr<const FStateTreeConditionBase>())
		{
			if (Condition->bInstanceIsObject)
			{
				DataViews[Condition->DataViewIndex.Get()] = SharedInstanceData.GetMutableObject(Condition->InstanceIndex.Get());
			}
			else
			{
				DataViews[Condition->DataViewIndex.Get()] = SharedInstanceData.GetMutableStruct(Condition->InstanceIndex.Get());
			}
		}
		else
		{
			const FStateTreeNodeBase& Node = NodeView.Get<const FStateTreeNodeBase>();
			if (Node.bInstanceIsObject)
			{
				DataViews[Node.DataViewIndex.Get()] = DefaultInstanceData.GetMutableObject(Node.InstanceIndex.Get());
			}
			else
			{
				DataViews[Node.DataViewIndex.Get()] = DefaultInstanceData.GetMutableStruct(Node.InstanceIndex.Get());
			}
		}
	}

	for (int32 BatchIndex = 0; BatchIndex < CopyBatches.Num(); ++BatchIndex)
	{
		const FStateTreePropertyCopyBatch& Batch = CopyBatches[BatchIndex];

		// Find data view for the binding target.
		FStateTreeDataView TargetView;

		// Target can be one of the nodes.
		for (FConstStructView NodeView : GetNodes())
		{
			const FStateTreeNodeBase& Node = NodeView.Get<const FStateTreeNodeBase>();
			if (Node.BindingsBatch.AsInt32() == BatchIndex)
			{
				TargetView = DataViews[Node.DataViewIndex.Get()];
				break;
			}
		}

		// Target can be a linked state parameters (subtree state params can only be a binding source).
		if (!TargetView.IsValid())
		{
			for (const FCompactStateTreeState& State : GetStates())
			{
				if (State.Type == EStateTreeStateType::Linked)
				{
					const FCompactStateTreeParameters& Params = DefaultInstanceData.GetStruct(State.ParameterInstanceIndex.Get()).Get<const FCompactStateTreeParameters>();
					if (Params.BindingsBatch.Get() == BatchIndex)
					{
						const FCompactStateTreeState& LinkedState = States[State.LinkedState.Index];
						TargetView = DataViews[LinkedState.ParameterDataViewIndex.Get()];
					}
				}
			}
		}

		if (!TargetView.IsValid())
		{
			UE_LOG(LogStateTree, Error, TEXT("%hs: Invalid target struct when trying to bind to '%s'."), __FUNCTION__, *Batch.TargetStruct.Name.ToString());
			return false;
		}

		for (int32 Index = Batch.BindingsBegin; Index != Batch.BindingsEnd; Index++)
		{
			FStateTreePropertyPathBinding& Binding = PropertyPathBindings[Index];

			if (!Binding.GetMutableSourcePath().UpdateSegmentsFromValue(DataViews[Binding.GetCompiledSourceStructIndex().Get()]))
			{
				UE_LOG(LogStateTree, Error, TEXT("%hs: Failed to update source instance structs for property binding '%s'."), __FUNCTION__, *Binding.GetTargetPath().ToString());
				return false;
			}

			if (!Binding.GetMutableTargetPath().UpdateSegmentsFromValue(TargetView))
			{
				UE_LOG(LogStateTree, Error, TEXT("%hs: Failed to update target instance structs for property binding '%s'."), __FUNCTION__, *Binding.GetTargetPath().ToString());
				return false;
			}
		}
	}

	return true;
}

#if WITH_EDITOR

void FStateTreeMemoryUsage::AddUsage(const FConstStructView View)
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

	if (!bIsLinked
		|| States.IsEmpty()
		|| !Nodes.IsValid()
		|| !DefaultInstanceData.IsValid())
	{
		return MemoryUsages;
	}

	const int32 TreeMemUsageIndex = MemoryUsages.Emplace(TEXT("State Tree Max"));
	const int32 InstanceMemUsageIndex = MemoryUsages.Emplace(TEXT("Instance Overhead"));
	const int32 EvalMemUsageIndex = MemoryUsages.Emplace(TEXT("Evaluators"));
	const int32 GlobalTaskMemUsageIndex = MemoryUsages.Emplace(TEXT("GlobalTask"));
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
			if (const FStateTreeTaskBase* Task = Nodes[TaskIndex].GetPtr<const FStateTreeTaskBase>())
			{
				if (Task->bInstanceIsObject)
				{
					MemUsage.NodeCount++;
					MemUsage.AddUsage(DefaultInstanceData.GetObject(Task->InstanceIndex.Get()));
				}
				else
				{
					MemUsage.NodeCount++;
					MemUsage.AddUsage(DefaultInstanceData.GetStruct(Task->InstanceIndex.Get()));
				}
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
		const FStateTreeEvaluatorBase& Eval = Nodes[EvalIndex].Get<const FStateTreeEvaluatorBase>();
		if (Eval.bInstanceIsObject == false)
		{
			EvalMemUsage.AddUsage(DefaultInstanceData.GetStruct(Eval.InstanceIndex.Get()));
		}
		else
		{
			EvalMemUsage.AddUsage(DefaultInstanceData.GetObject(Eval.InstanceIndex.Get()));
		}
		EvalMemUsage.NodeCount++;
	}

	// Global Tasks
	FStateTreeMemoryUsage& GlobalTaskMemUsage = MemoryUsages[GlobalTaskMemUsageIndex];
	for (int32 TaskIndex = GlobalTasksBegin; TaskIndex < (GlobalTasksBegin + GlobalTasksNum); TaskIndex++)
	{
		const FStateTreeTaskBase& Task = Nodes[TaskIndex].Get<const FStateTreeTaskBase>();
		if (Task.bInstanceIsObject == false)
		{
			GlobalTaskMemUsage.AddUsage(DefaultInstanceData.GetStruct(Task.InstanceIndex.Get()));
		}
		else
		{
			GlobalTaskMemUsage.AddUsage(DefaultInstanceData.GetObject(Task.InstanceIndex.Get()));
		}
		GlobalTaskMemUsage.NodeCount++;
	}

	// Estimate highest combined usage.
	FStateTreeMemoryUsage& TreeMemUsage = MemoryUsages[TreeMemUsageIndex];

	// Exec state
	TreeMemUsage.AddUsage(DefaultInstanceData.GetStruct(0));
	TreeMemUsage.NodeCount++;

	TreeMemUsage.EstimatedMemoryUsage += EvalMemUsage.EstimatedMemoryUsage;
	TreeMemUsage.NodeCount += EvalMemUsage.NodeCount;

	TreeMemUsage.EstimatedMemoryUsage += GlobalTaskMemUsage.EstimatedMemoryUsage;
	TreeMemUsage.NodeCount += GlobalTaskMemUsage.NodeCount;

	FStateTreeMemoryUsage& InstanceMemUsage = MemoryUsages[InstanceMemUsageIndex];
	// FStateTreeInstanceData overhead.
	InstanceMemUsage.EstimatedMemoryUsage += sizeof(FStateTreeInstanceData);
	// FInstancedStructContainer overhead.
	InstanceMemUsage.EstimatedMemoryUsage += TreeMemUsage.NodeCount * FInstancedStructContainer::OverheadPerItem;

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

