// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "InstancedStructArray.h"
#include "StructView.h"
#include "StateTreeEvents.h"
#include "StateTreeInstanceData.generated.h"

/**
 * StateTree instance data is used to store the runtime state of a StateTree.
 * The layout of the data is described in a FStateTreeInstanceDataLayout.
 *
 * Note: Serialization is supported only for FArchive::IsModifyingWeakAndStrongReferences(), that is replacing object references.
 */
USTRUCT()
struct STATETREEMODULE_API FStateTreeInstanceData
{
	GENERATED_BODY()

	FStateTreeInstanceData() = default;
	~FStateTreeInstanceData() { Reset(); }

	/** Initializes the array with specified items. */
	void Init(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, TConstArrayView<UObject*> InObjects);
	void Init(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<UObject*> InObjects);

	/** Appends new items to the instance. */
	void Append(UObject& InOwner, TConstArrayView<FInstancedStruct> InStructs, TConstArrayView<UObject*> InObjects);
	void Append(UObject& InOwner, TConstArrayView<FConstStructView> InStructs, TConstArrayView<UObject*> InObjects);

	/** Prunes the array sizes to specified lengths. */
	void Prune(const int32 NumStructs, const int32 NumObjects);

	/** Shares the layout from another instance data, and copies the data over. */
	void CopyFrom(UObject& InOwner, const FStateTreeInstanceData& InOther);

	/** Resets the data to empty. */
	void Reset();

	/** @return true if the instance is correctly initialized. */
	bool IsValid() const { return InstanceStructs.Num() > 0 || InstanceObjects.Num() > 0; }

	/** @return Number of items in the instance data. */
	int32 NumStructs() const { return InstanceStructs.Num(); }

	/** @return mutable view to the struct at specified index. */
	FStructView GetMutableStruct(const int32 Index) const { return InstanceStructs[Index]; }

	/** @return const view to the struct at specified index. */
	FConstStructView GetStruct(const int32 Index) const { return InstanceStructs[Index]; }

	/** @return number of instance objects */
	int32 NumObjects() const { return InstanceObjects.Num(); }

	/** @return pointer to an instance object   */
	UObject* GetMutableObject(const int32 Index) const { return InstanceObjects[Index]; }

	/** @return const pointer to an instance object   */
	const UObject* GetObject(const int32 Index) const { return InstanceObjects[Index]; }

	/** @return array to store unprocessed events. */
	TArray<FStateTreeEvent>& GetEvents() { return Events; }
	
	int32 GetEstimatedMemoryUsage() const;
	int32 GetNumItems() const;
	
	/** Type traits */
	bool Identical(const FStateTreeInstanceData* Other, uint32 PortFlags) const;
	
private:
	
	/** Struct instances */
	UPROPERTY()
	FInstancedStructArray InstanceStructs;

	/** Object instances. */
	UPROPERTY()
	TArray<TObjectPtr<UObject>> InstanceObjects;

	/** Events */
	UPROPERTY()
	TArray<FStateTreeEvent> Events;
};

template<>
struct TStructOpsTypeTraits<FStateTreeInstanceData> : public TStructOpsTypeTraitsBase2<FStateTreeInstanceData>
{
	enum
	{
		WithIdentical = true,
	};
};
