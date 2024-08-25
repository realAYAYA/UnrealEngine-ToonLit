// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStructContainer.h"
#include "StateTreeEvents.h"
#include "StateTreeTypes.h"
#include "StateTreeExecutionTypes.h"
#include "Templates/SharedPointer.h"
#include "StateTreeInstanceData.generated.h"

struct FStateTreeTransitionRequest;
struct FStateTreeExecutionState;

/** Wrapper class to store an object amongst the structs. */
USTRUCT()
struct STATETREEMODULE_API FStateTreeInstanceObjectWrapper
{
	GENERATED_BODY()

	FStateTreeInstanceObjectWrapper() = default;
	FStateTreeInstanceObjectWrapper(UObject* Object) : InstanceObject(Object) {}
	
	UPROPERTY()
	TObjectPtr<UObject> InstanceObject = nullptr;
};

/**
 * Holds temporary instance data created during state selection.
 * The data is identified by Frame (StateTree + RootState) and DataHandle.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeTemporaryInstanceData
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<const UStateTree> StateTree = nullptr;

	UPROPERTY()
	FStateTreeStateHandle RootState = FStateTreeStateHandle::Invalid;

	UPROPERTY()
	FStateTreeDataHandle DataHandle = FStateTreeDataHandle::Invalid;

	UPROPERTY()
	FStateTreeIndex16 OwnerNodeIndex = FStateTreeIndex16::Invalid; 
	
	UPROPERTY()
	FInstancedStruct Instance;
};

struct STATETREEMODULE_API FStateTreeInstanceStorageCustomVersion
{
	enum Type
	{
		// Before any version changes were made in the plugin
		BeforeCustomVersionWasAdded = 0,
		// Added custom serialization
		AddedCustomSerialization,

		// -----<new versions can be added above this line>-------------------------------------------------
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	/** The GUID for this custom version number */
	const static FGuid GUID;

private:
	FStateTreeInstanceStorageCustomVersion() = default;
};

/**
 * State Tree instance data is used to store the runtime state of a State Tree. It is used together with FStateTreeExecution context to tick the state tree.
 * You are supposed to use FStateTreeInstanceData as a property to store the instance data. That ensures that any UObject references will get GC'd correctly.
 *
 * The FStateTreeInstanceData wraps FStateTreeInstanceStorage, where the data is actually stored. This indirection is done in order to allow the FStateTreeInstanceData
 * to be bitwise relocatable (e.g. you can put it in an array), and we can still allow delegates to bind to the instance data of individual tasks.
 *
 * Since the tasks in the instance data are stored in a array that may get resized you will need to use TStateTreeInstanceDataStructRef
 * to reference a struct based task instance data. It is defined below, and has example how to use it.
 */

/** Storage for the actual instance data. */
USTRUCT()
struct STATETREEMODULE_API FStateTreeInstanceStorage
{
	GENERATED_BODY()

	/** @return reference to the event queue. */
	FStateTreeEventQueue& GetMutableEventQueue()
	{
		return EventQueue;
	}

	/** @return reference to the event queue. */
	const FStateTreeEventQueue& GetEventQueue() const
	{
		return EventQueue;
	}

	/** 
	 * Buffers a transition request to be sent to the State Tree.
	 * @param Owner Optional pointer to an owner UObject that is used for logging errors.
	 * @param Request transition to request.
	*/
	void AddTransitionRequest(const UObject* Owner, const FStateTreeTransitionRequest& Request);

	/** @return currently pending transition requests. */
	TConstArrayView<FStateTreeTransitionRequest> GetTransitionRequests() const
	{
		return TransitionRequests;
	}

	/** Reset all pending transition requests. */
	void ResetTransitionRequests();

	/** @return true if all instances are valid. */
	bool AreAllInstancesValid() const;

	/** @return number of items in the storage. */
	int32 Num() const
	{
		return InstanceStructs.Num();
	}

	/** @return true if the index can be used to get data. */
	bool IsValidIndex(const int32 Index) const
	{
		return InstanceStructs.IsValidIndex(Index);
	}

	/** @return true if item at the specified index is object type. */
	bool IsObject(const int32 Index) const
	{
		return InstanceStructs[Index].GetScriptStruct() == TBaseStructure<FStateTreeInstanceObjectWrapper>::Get();
	}

	/** @return specified item as struct. */
	FConstStructView GetStruct(const int32 Index) const
	{
		return InstanceStructs[Index];
	}
	
	/** @return specified item as mutable struct. */
	FStructView GetMutableStruct(const int32 Index)
	{
		return InstanceStructs[Index];
	}

	/** @return specified item as object, will check() if the item is not an object. */
	const UObject* GetObject(const int32 Index) const
	{
		const FStateTreeInstanceObjectWrapper& Wrapper = InstanceStructs[Index].Get<const FStateTreeInstanceObjectWrapper>();
		return Wrapper.InstanceObject;
	}

	/** @return specified item as mutable Object, will check() if the item is not an object. */
	UObject* GetMutableObject(const int32 Index) const
	{
		const FStateTreeInstanceObjectWrapper& Wrapper = InstanceStructs[Index].Get<const FStateTreeInstanceObjectWrapper>();
		return Wrapper.InstanceObject;
	}

	/** @return reference to StateTree execution state, or null if the instance data is not initialized. */
	const FStateTreeExecutionState& GetExecutionState() const
	{
		return ExecutionState;
	}

	/** @return reference to StateTree execution state, or null if the instance data is not initialized. */
	FStateTreeExecutionState& GetMutableExecutionState()
	{
		return ExecutionState;
	}

	/**
	 * Adds temporary instance data associated with specified frame and data handle.
	 * @returns mutable struct view to the instance.
	 */
	FStructView AddTemporaryInstance(UObject& InOwner, const FStateTreeExecutionFrame& Frame, const FStateTreeIndex16 OwnerNodeIndex, const FStateTreeDataHandle DataHandle, FConstStructView NewInstanceData);
	
	/** @returns mutable view to the specified instance data, or invalid view if not found. */
	FStructView GetMutableTemporaryStruct(const FStateTreeExecutionFrame& Frame, const FStateTreeDataHandle DataHandle);

	/** @returns mutable pointer to the specified instance data object, or invalid view if not found. Will check() if called on non-object data. */
	UObject* GetMutableTemporaryObject(const FStateTreeExecutionFrame& Frame, const FStateTreeDataHandle DataHandle);

	/** Empties the temporary instances. */
	void ResetTemporaryInstances();

	/** @return mutable array view to the temporary instances */
	TArrayView<FStateTreeTemporaryInstanceData> GetMutableTemporaryInstances()
	{
		return TemporaryInstances;
	}

	/** Stores copy of provided parameters as State Tree global parameters. */
	void SetGlobalParameters(const FInstancedPropertyBag& Parameters);

	/** @return view to global parameters. */
	FConstStructView GetGlobalParameters() const
	{
		return GlobalParameters.GetValue();
	}

	/** @return mutable view to global parameters. */
	FStructView GetMutableGlobalParameters()
	{
		return GlobalParameters.GetMutableValue();
	}
	
	UE_DEPRECATED(5.4, "Use Num() instead.")
	int32 NumStructs() const { return 0; }

	UE_DEPRECATED(5.4, "Use IsValidIndex() instead.")
	bool IsValidStructIndex(const int32 Index) const { return false; }
	
	UE_DEPRECATED(5.4, "Use Num() instead.")
	int32 NumObjects() const { return 0; }

	UE_DEPRECATED(5.4, "Not used anymore, since ExecutionState is stored separately.")
	bool IsValid() const { return false; }

protected:
	/** Execution state of the state tree instance. */
	UPROPERTY()
	FStateTreeExecutionState ExecutionState;

	/** Struct instances */
	UPROPERTY()
	FInstancedStructContainer InstanceStructs;

	/** Temporary instances */
	UPROPERTY()
	TArray<FStateTreeTemporaryInstanceData> TemporaryInstances;

	/** Events */
	UPROPERTY()
	FStateTreeEventQueue EventQueue;

	/** Requested transitions */
	UPROPERTY()
	TArray<FStateTreeTransitionRequest> TransitionRequests;

	/** Global parameters */
	UPROPERTY()
	FInstancedPropertyBag GlobalParameters;

	friend struct FStateTreeInstanceData;
};

/**
 * StateTree instance data is used to store the runtime state of a StateTree.
 * The layout of the data is described in a FStateTreeInstanceDataLayout.
 *
 * Note: If FStateTreeInstanceData is placed on an struct, you must call AddStructReferencedObjects() manually,
 *		 as it is not automatically called recursively.   
 * Note: Serialization is supported only for FArchive::IsModifyingWeakAndStrongReferences(), that is replacing object references.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeInstanceData
{
	GENERATED_BODY()

	FStateTreeInstanceData();
	FStateTreeInstanceData(const FStateTreeInstanceData& Other);
	FStateTreeInstanceData(FStateTreeInstanceData&& Other);

	FStateTreeInstanceData& operator=(const FStateTreeInstanceData& Other);
	FStateTreeInstanceData& operator=(FStateTreeInstanceData&& Other);

	~FStateTreeInstanceData();
	
	/** Initializes the array with specified items. */
	void Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs);
	void Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs);

	/** Appends new items to the instance. */
	void Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs);
	void Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs);

	/** Appends new items to the instance, and moves existing data into the allocated instances. */
	void Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<FInstancedStruct*> InInstancesToMove);

	/** Shrinks the array sizes to specified lengths. Sizes must be small or equal than current size. */
	void ShrinkTo(const int32 Num);
	
	/** Shares the layout from another instance data, and copies the data over. */
	void CopyFrom(UObject& InOwner, const FStateTreeInstanceData& InOther);

	/** Resets the data to empty. */
	void Reset();

	/** @return Number of items in the instance data. */
	int32 Num() const
	{
		return GetStorage().Num();
	}

	/** @return true if the specified index is valid index into the instance data container. */
	bool IsValidIndex(const int32 Index) const
	{
		return GetStorage().IsValidIndex(Index);
	}

	/** @return true if the data at specified index is object. */
	bool IsObject(const int32 Index) const
	{
		return GetStorage().IsObject(Index);
	}

	/** @return mutable view to the struct at specified index. */
	FStructView GetMutableStruct(const int32 Index)
	{
		return GetMutableStorage().GetMutableStruct(Index);
	}

	/** @return const view to the struct at specified index. */
	FConstStructView GetStruct(const int32 Index) const
	{
		return GetStorage().GetStruct(Index);
	}

	/** @return pointer to an instance object   */
	UObject* GetMutableObject(const int32 Index)
	{
		return GetMutableStorage().GetMutableObject(Index);
	}

	/** @return const pointer to an instance object   */
	const UObject* GetObject(const int32 Index) const
	{
		return GetStorage().GetObject(Index);
	}

	/** @return pointer to StateTree execution state, or null if the instance data is not initialized. */
	const FStateTreeExecutionState* GetExecutionState() const
	{
		return &GetStorage().GetExecutionState();
	}

	/** @return mutable pointer to StateTree execution state, or null if the instance data is not initialized. */
	FStateTreeExecutionState* GetMutableExecutionState()
	{
		return &GetMutableStorage().GetMutableExecutionState();
	}

	/** @return reference to the event queue. */
	FStateTreeEventQueue& GetMutableEventQueue();
	const FStateTreeEventQueue& GetEventQueue() const;

	/** 
	 * Buffers a transition request to be sent to the State Tree.
	 * @param Owner Optional pointer to an owner UObject that is used for logging errors.
	 * @param Request transition to request.
	*/
	void AddTransitionRequest(const UObject* Owner, const FStateTreeTransitionRequest& Request);

	/** @return currently pending transition requests. */
	TConstArrayView<FStateTreeTransitionRequest> GetTransitionRequests() const;

	/** Reset all pending transition requests. */
	void ResetTransitionRequests();

	/** @return true if all instances are valid. */
	bool AreAllInstancesValid() const;

	FStateTreeInstanceStorage& GetMutableStorage();
	const FStateTreeInstanceStorage& GetStorage() const;

	TWeakPtr<FStateTreeInstanceStorage> GetWeakMutableStorage();
	TWeakPtr<const FStateTreeInstanceStorage> GetWeakStorage() const;

	int32 GetEstimatedMemoryUsage() const;
	
	/** Type traits */
	bool Identical(const FStateTreeInstanceData* Other, uint32 PortFlags) const;
	void AddStructReferencedObjects(FReferenceCollector& Collector);
	bool Serialize(FArchive& Ar);
	void GetPreloadDependencies(TArray<UObject*>& OutDeps);

	/**
	 * Adds temporary instance data associated with specified frame and data handle.
	 * @returns mutable struct view to the instance.
	 */
	FStructView AddTemporaryInstance(UObject& InOwner, const FStateTreeExecutionFrame& Frame, const FStateTreeIndex16 OwnerNodeIndex, const FStateTreeDataHandle DataHandle, FConstStructView NewInstanceData)
	{
		return GetMutableStorage().AddTemporaryInstance(InOwner, Frame, OwnerNodeIndex, DataHandle, NewInstanceData);
	}
	
	/** @returns mutable view to the specified instance data, or invalid view if not found. */
	FStructView GetMutableTemporaryStruct(const FStateTreeExecutionFrame& Frame, const FStateTreeDataHandle DataHandle)
	{
		return GetMutableStorage().GetMutableTemporaryStruct(Frame, DataHandle);
	}

	/** @returns mutable pointer to the specified instance data object, or invalid view if not found. Will check() if called on non-object data. */
	UObject* GetMutableTemporaryObject(const FStateTreeExecutionFrame& Frame, const FStateTreeDataHandle DataHandle)
	{
		return GetMutableStorage().GetMutableTemporaryObject(Frame, DataHandle);
	}

	/** Empties the temporary instances. */
	void ResetTemporaryInstances()
	{
		return GetMutableStorage().ResetTemporaryInstances();
	}
	
	UE_DEPRECATED(5.4, "Use the structs only Init(), objects should be wrapped in FStateTreeInstanceObjectWrapper.")
	void Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, TConstArrayView<const UObject*> InObjects);

	UE_DEPRECATED(5.4, "Use the structs only Init(), objects should be wrapped in FStateTreeInstanceObjectWrapper.")
	void Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<const UObject*> InObjects);

	UE_DEPRECATED(5.4, "Use the structs only Init(), objects should be wrapped in FStateTreeInstanceObjectWrapper.")
	void Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, TConstArrayView<const UObject*> InObjects);

	UE_DEPRECATED(5.4, "Use the structs only Init(), objects should be wrapped in FStateTreeInstanceObjectWrapper.")
	void Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<const UObject*> InObjects);

	UE_DEPRECATED(5.4, "Use the one param ShrinkTo().")
	void ShrinkTo(const int32 NumStructs, const int32 NumObjects);

	UE_DEPRECATED(5.4, "Use IsValidIndex() instead.")
	bool IsValidStructIndex(const int32 Index) const { return false; }

	UE_DEPRECATED(5.4, "Use Num() instead.")
	int32 NumStructs() const { return GetStorage().InstanceStructs.Num(); }
	
	UE_DEPRECATED(5.4, "Use Num() instead.")
	int32 NumObjects() const { return 0; }

	UE_DEPRECATED(5.4, "Use Num() instead.")
	int32 GetNumItems() const { return 0; }

	UE_DEPRECATED(5.4, "InstanceData is always valid.")
	bool IsValid() const { return true; }

protected:
	/** Storage for the actual instance data, always stores FStateTreeInstanceStorage. */
	TSharedRef<FStateTreeInstanceStorage> InstanceStorage = MakeShared<FStateTreeInstanceStorage>();

#if WITH_EDITORONLY_DATA
PRAGMA_DISABLE_DEPRECATION_WARNINGS

	UPROPERTY()
	TInstancedStruct<FStateTreeInstanceStorage> InstanceStorage_DEPRECATED;
	
PRAGMA_ENABLE_DEPRECATION_WARNINGS
#endif // WITH_EDITORONLY_DATA
};

#if WITH_EDITORONLY_DATA
namespace UE::StateTree
{
	void RegisterInstanceDataForLocalization();
}
#endif // WITH_EDITORONLY_DATA

template<>
struct TStructOpsTypeTraits<FStateTreeInstanceData> : public TStructOpsTypeTraitsBase2<FStateTreeInstanceData>
{
	enum
	{
		WithIdentical = true,
		WithAddStructReferencedObjects = true,
		WithSerializer = true,
		WithGetPreloadDependencies = true,
	};
};


/**
 * Stores indexed reference to a instance data struct.
 * The instance data structs may be relocated when the instance data composition changed. For that reason you cannot store pointers to the instance data.
 * This is often needed for example when dealing with delegate lambda's. This helper struct stores data to be able to find the instance data in the instance data array.
 * That way we can access the instance data even of the array changes, and the instance data moves in memory.
 *
 * Note that the reference is valid only during the lifetime of a task (between a call EnterState() and ExitState()). 
 *
 * You generally do not use this directly, but via FStateTreeExecutionContext.
 *
 *	EStateTreeRunStatus FTestTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
 *	{
 *		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
 *
 *		Context.GetWorld()->GetTimerManager().SetTimer(
 *	        InstanceData.TimerHandle,
 *	        [InstanceDataRef = Context.GetInstanceDataStructRef(*this)]()
 *	        {
 *	            if (FInstanceDataType* InstanceData = InstanceDataRef.GetPtr())
 *				{
 *		            ...
 *				}
 *	        },
 *	        Delay, true);
 *
 *	    return EStateTreeRunStatus::Running;
 *	}
 */
template <typename T>
struct TStateTreeInstanceDataStructRef
{
	TStateTreeInstanceDataStructRef(FStateTreeInstanceData& InInstanceData, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle InDataHandle)
		: WeakStorage(InInstanceData.GetWeakMutableStorage())
		, WeakStateTree(CurrentFrame.StateTree)
		, RootState(CurrentFrame.RootState)
		, DataHandle(InDataHandle)
	{
		checkf(InDataHandle.GetSource() == EStateTreeDataSourceType::ActiveInstanceData
			|| InDataHandle.GetSource() == EStateTreeDataSourceType::GlobalInstanceData,
			TEXT("TStateTreeInstanceDataStructRef supports only struct instance data."));
	}

	bool IsValid() const { return WeakStateTree.IsValid() && RootState.IsValid() && DataHandle.IsValid(); }

	T* GetPtr()
	{
		if (!WeakStorage.IsValid())
		{
			return nullptr;
		}

		FStateTreeInstanceStorage& Storage = *WeakStorage.Pin();

		const FStateTreeExecutionState& Exec = Storage.GetExecutionState();
		const UStateTree* StateTree = WeakStateTree.Get();
		
		const FStateTreeExecutionFrame* CurrentFrame = Exec.ActiveFrames.FindByPredicate([StateTree, RootState = RootState](const FStateTreeExecutionFrame& Frame)
		{
			return Frame.StateTree == StateTree && Frame.RootState == RootState;
		});

		FStructView Struct;
		if (CurrentFrame)
		{
			if (IsHandleSourceValid(Storage, *CurrentFrame, DataHandle))
			{
				Struct = GetDataView(Storage, *CurrentFrame, DataHandle);
			}
			else
			{
				Struct = Storage.GetMutableTemporaryStruct(*CurrentFrame, DataHandle);
			}
		}

		check(Struct.GetScriptStruct() == TBaseStructure<T>::Get());
		return reinterpret_cast<T*>(Struct.GetMemory());
	}

	UE_DEPRECATED(5.4, "Please use GetPtr(), as the ref may be invalidated while in use.")
	T& operator*()
	{
		T* Result = GetPtr();
		check(Result);
		return *Result;
	}

protected:

	FStructView GetDataView(FStateTreeInstanceStorage& Storage, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle)
	{
		switch (DataHandle.GetSource())
		{
		case EStateTreeDataSourceType::GlobalInstanceData:
			return Storage.GetMutableStruct(CurrentFrame.GlobalInstanceIndexBase.Get() + DataHandle.GetIndex());
		case EStateTreeDataSourceType::ActiveInstanceData:
			return Storage.GetMutableStruct(CurrentFrame.ActiveInstanceIndexBase.Get() + DataHandle.GetIndex());
		default:
			checkf(false, TEXT("Unhandle case %s"), *UEnum::GetValueAsString(Handle.GetSource()));
		}
		return {};
	}
	
	bool IsHandleSourceValid(FStateTreeInstanceStorage& Storage, const FStateTreeExecutionFrame& CurrentFrame, const FStateTreeDataHandle Handle) const
	{
		switch (Handle.GetSource())
		{
		case EStateTreeDataSourceType::GlobalInstanceData:
			return CurrentFrame.GlobalInstanceIndexBase.IsValid()
				&& Storage.IsValidIndex(CurrentFrame.GlobalInstanceIndexBase.Get() + Handle.GetIndex());

		case EStateTreeDataSourceType::ActiveInstanceData:
			return CurrentFrame.ActiveInstanceIndexBase.IsValid()
				&& CurrentFrame.ActiveStates.Contains(Handle.GetState())
				&& Storage.IsValidIndex(CurrentFrame.ActiveInstanceIndexBase.Get() + Handle.GetIndex());
		default:
			checkf(false, TEXT("Unhandle case %s"), *UEnum::GetValueAsString(Handle.GetSource()));
		}
		return false;
	}
	
	TWeakPtr<FStateTreeInstanceStorage> WeakStorage = nullptr;
	TWeakObjectPtr<const UStateTree> WeakStateTree = nullptr;
	FStateTreeStateHandle RootState = FStateTreeStateHandle::Invalid;
	FStateTreeDataHandle DataHandle = FStateTreeDataHandle::Invalid;
};
