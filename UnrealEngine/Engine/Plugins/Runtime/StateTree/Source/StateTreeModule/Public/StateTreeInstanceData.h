// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStructContainer.h"
#include "StateTreeEvents.h"
#include "StateTreeTypes.h"
#include "StateTreeExecutionTypes.h"
#include "StateTreeInstanceData.generated.h"

struct FStateTreeTransitionRequest;
struct FStateTreeExecutionState;

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

	/** @return true if the instance is correctly initialized. */
	bool IsValid() const;

	/** @return Number of items in the instance data. */
	int32 NumStructs() const { return InstanceStructs.Num(); }

	/** @return true if the specified index is valid index into the struct data container. */
	bool IsValidStructIndex(const int32 Index) const { return InstanceStructs.IsValidIndex(Index); }
	
	/** @return mutable view to the struct at specified index. */
	FStructView GetMutableStruct(const int32 Index) { return InstanceStructs[Index]; }

	/** @return const view to the struct at specified index. */
	FConstStructView GetStruct(const int32 Index) const { return InstanceStructs[Index]; }

	/** @return number of instance objects */
	int32 NumObjects() const { return InstanceObjects.Num(); }

	/** @return pointer to an instance object   */
	UObject* GetMutableObject(const int32 Index) const { return InstanceObjects[Index]; }

	/** @return const pointer to an instance object   */
	const UObject* GetObject(const int32 Index) const { return InstanceObjects[Index]; }

	/** @return reference to the event queue. */
	FStateTreeEventQueue& GetMutableEventQueue() { return EventQueue; }

	/** @return reference to the event queue. */
	const FStateTreeEventQueue& GetEventQueue() const { return EventQueue; }

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

protected:
	/** Struct instances */
	UPROPERTY()
	FInstancedStructContainer InstanceStructs;

	/** Object instances. */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> InstanceObjects;

	/** Events */
	UPROPERTY()
	FStateTreeEventQueue EventQueue;

	/** Requested transitions */
	UPROPERTY()
	TArray<FStateTreeTransitionRequest> TransitionRequests;

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
	~FStateTreeInstanceData();

	/** Initializes the array with specified items. */
	void Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, TConstArrayView<const UObject*> InObjects);
	void Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<const UObject*> InObjects);

	/** Appends new items to the instance. */
	void Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, TConstArrayView<const UObject*> InObjects);
	void Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<const UObject*> InObjects);

	/** Shrinks the array sizes to specified lengths. Sizes must be small or equal than current size. */
	void ShrinkTo(const int32 NumStructs, const int32 NumObjects);

	/** Shares the layout from another instance data, and copies the data over. */
	void CopyFrom(UObject& InOwner, const FStateTreeInstanceData& InOther);

	/** Resets the data to empty. */
	void Reset();

	/** @return true if the instance is correctly initialized. */
	bool IsValid() const;

	/** @return Number of items in the instance data. */
	int32 NumStructs() const { return GetStorage().InstanceStructs.Num(); }

	/** @return true if the specified index is valid index into the struct data container. */
	bool IsValidStructIndex(const int32 Index) const { return GetStorage().InstanceStructs.IsValidIndex(Index); }
	
	/** @return mutable view to the struct at specified index. */
	FStructView GetMutableStruct(const int32 Index) { return GetMutableStorage().InstanceStructs[Index]; }

	/** @return const view to the struct at specified index. */
	FConstStructView GetStruct(const int32 Index) const { return GetStorage().InstanceStructs[Index]; }

	/** @return number of instance objects */
	int32 NumObjects() const { return GetStorage().InstanceObjects.Num(); }

	/** @return pointer to an instance object   */
	UObject* GetMutableObject(const int32 Index) { return GetMutableStorage().InstanceObjects[Index]; }

	/** @return const pointer to an instance object   */
	const UObject* GetObject(const int32 Index) const { return GetStorage().InstanceObjects[Index]; }

	/** @return pointer to StateTree execution state, or null if the instance data is not initialized. */
	const FStateTreeExecutionState* GetExecutionState() const;
	
	/** @return array to store unprocessed events. */
	UE_DEPRECATED(5.2, "Use GetEventQueue() instead.")
	TArray<FStateTreeEvent>& GetEvents() const;

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

	int32 GetEstimatedMemoryUsage() const;
	int32 GetNumItems() const;
	
	/** Type traits */
	bool Identical(const FStateTreeInstanceData* Other, uint32 PortFlags) const;
	void PostSerialize(const FArchive& Ar);

protected:

	/** Storage for the actual instance data, always stores FStateTreeInstanceStorage. */
	UPROPERTY()
	FInstancedStruct InstanceStorage;

#if WITH_EDITORONLY_DATA
	UPROPERTY(meta = (DeprecatedProperty))
	FInstancedStructContainer InstanceStructs_DEPRECATED;

	UPROPERTY(meta = (DeprecatedProperty))
	TArray<TObjectPtr<UObject>> InstanceObjects_DEPRECATED;
#endif // WITH_EDITORONLY_DATA
};

template<>
struct TStructOpsTypeTraits<FStateTreeInstanceData> : public TStructOpsTypeTraitsBase2<FStateTreeInstanceData>
{
	enum
	{
		WithIdentical = true,
		WithPostSerialize = true,
	};
};


/**
 * Stores indexed reference to a instance data struct.
 * The instance data structs may be relocated when the instance data composition changed. For that reason you cannot store pointers to the instance data.
 * This is often needed for example when dealing with delegate lambda's. This helper struct stores the instance data as index to the instance data array.
 * That way we can access the instance data even of the array changes.
 *
 * Note that the reference is valid only during the lifetime of a task (between EnterState() and ExitState()). 
 *
 * You generally do not use this directly, but via FStateTreeExecutionContext.
 *
 *	EStateTreeRunStatus FTestTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
 *	{
 *		FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
 *
 *		Context.GetWorld()->GetTimerManager().SetTimer(
 *	        InstanceData.TimerHandle,
 *	        [InstanceDataRef = Context.GetInstanceDataStructRef()]()
 *	        {
 *	            FInstanceDataType& InstanceData = *InstanceDataRef;
 *	            ...
 *	        },
 *	        Delay, true);
 *
 *	    return EStateTreeRunStatus::Running;
 *	}
 */
template <typename T>
struct TStateTreeInstanceDataStructRef
{
	TStateTreeInstanceDataStructRef(FStateTreeInstanceData& InInstanceData, const T& InstanceDataStruct)
		: Storage(InInstanceData.GetMutableStorage())
	{
		const FConstStructView InstanceDataStructView = FConstStructView::template Make(InstanceDataStruct);
		// Find struct in the instance data.
		for (int32 Index = 0; Index < Storage.NumStructs(); Index++)
		{
			if (Storage.GetStruct(Index) == InstanceDataStructView)
			{
				StructIndex = Index;
				break;
			}
		}
		check(StructIndex != INDEX_NONE);
	}

	bool IsValid() const { return Storage.IsValidStructIndex(StructIndex); }

	T& operator*()
	{
		check(IsValid());
		FStructView Struct = Storage.GetMutableStruct(StructIndex);
		check(Struct.GetScriptStruct() == TBaseStructure<T>::Get());
		return *reinterpret_cast<T*>(Struct.GetMemory());
	}

protected:
	FStateTreeInstanceStorage& Storage;
	int32 StructIndex = INDEX_NONE;
};
