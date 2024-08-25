// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DataRegistryTypes.h"
#include "UObject/WeakObjectPtr.h"

class USkeletalMeshComponent;
struct FReferenceSkeleton;

namespace UE::AnimNext
{

class FModule;

struct FDataHandle;

// Global registry of animation data
// Holds ref counted data that gets released when the last DataHandle of that element goes out of scope
// Calling public functions from multiple threads is expected. Data races are guarded by a FRWLock.
// TODO : Memory management will have to be implemented to avoid fragmentation and performance reasons
class ANIMNEXT_API FDataRegistry
{
public:

	// Access the global registry
	static FDataRegistry* Get();


	// --- Reference Pose Handling ---

	// Generates and registers a reference pose for the SkeletalMesh asset of the SkeletalMeshComponent
	// and modifies it with the additional required bones 
	// or the visibility state of the bones of the SkeletalMeshComponent
	FDataHandle RegisterReferencePose(USkeletalMeshComponent* SkeletalMeshComponent);

	// Returns a ref counted handle to the refence pose of the given skeletal mesh component
	FDataHandle GetOrGenerateReferencePose(USkeletalMeshComponent* SkeletalMeshComponent);

	// Removes a previously registered reference pose for the given SkeletalMeshComponent
	void RemoveReferencePose(USkeletalMeshComponent* SkeletalMeshComponent);

	
	// --- AnimationData Storage / Retrieval  --- 

	// Registers an anim data handle with arbitrary data using an FName
	// Note that AnimDataHandles are refcounted, so this makes them permanent until unregistered
	void RegisterData(const FName& Id, const FDataHandle& AnimationDataHandle);

	// Unregisters a previously registered anim data handle 
	void UnregisterData(const FName& Id);

	// Obtains the data hanle for the passed Id, if it exists.
	// If there is no anim data handle registered, the handle IsValid will be false
	FDataHandle GetRegisteredData(const FName& Id) const;

	
	// --- Supported types registration ---

	// Registers a type and sets the desired preallocation block size
	// If a type is allocated without registering, a default block size of 32 will be used
	template<typename DataType>
	inline void RegisterDataType(int32 AllocationBlockSize)
	{
		RegisterDataType_Impl<DataType>(AllocationBlockSize);
	}

	// --- Persistent Data ---

	// Allocates uninitialized memory for a type (leaving the initialization to the caller)
	// Returns a refcounted animation data handle
	// Allocated memory will be released once the refcount reaches 0
	template<typename DataType>
	FDataHandle PreAllocateMemory(const int32 NumElements)
	{
		FParamTypeHandle ParamTypeHandle = FParamTypeHandle::GetHandle<DataType>();

		const FDataTypeDef* TypeDef = nullptr;
		{
			FRWScopeLock Lock(DataTypeDefsLock, SLT_ReadOnly);
			TypeDef = DataTypeDefs.Find(ParamTypeHandle);
		}

		if (TypeDef == nullptr)
		{
			TypeDef = RegisterDataType_Impl<DataType>(DEFAULT_BLOCK_SIZE);
			// TODO : Log if we allocate more than DEFAULT_BLOCK_SIZE elements of that type
		}

		if (ensure(TypeDef != nullptr && TypeDef->ParamTypeHandle.IsValid()))
		{
			const int32 ElementSize = TypeDef->ElementSize;
			const int32 ElementAlign = TypeDef->ElementAlign;
			const int32 AlignedSize = Align(ElementSize, ElementAlign);

			const int32 BufferSize = NumElements * AlignedSize;

			uint8* Memory = (uint8*)FMemory::Malloc(BufferSize, TypeDef->ElementAlign);    // TODO : This should come from preallocated chunks, use malloc / free for now

			Private::FAllocatedBlock* AllocatedBlock = new Private::FAllocatedBlock(Memory, NumElements, ParamTypeHandle); // TODO : avoid memory fragmentation
			AllocatedBlock->AddRef();

			FRWScopeLock Lock(AllocatedBlocksLock, SLT_Write);
			AllocatedBlocks.Add(AllocatedBlock);
			return FDataHandle(AllocatedBlock);
		}

		return FDataHandle();
	}

	// Allocates memory for a type, initialized with the default constructor (with optional passed arguments)
	// Returns a refcounted animation data handle
	// Allocated memory will be released once the refcount reaches 0
	template<typename DataType, typename... ArgTypes>
	FDataHandle AllocateData(const int32 NumElements, ArgTypes&&... Args)
	{
		FDataHandle Handle = PreAllocateMemory<DataType>(NumElements);

		DataType* RetVal = Handle.GetPtr<DataType>();
		for (int i = 0; i < NumElements; i++)
		{
			// perform a placement new per element
			new ((uint8*)&RetVal[i]) DataType(Forward<ArgTypes>(Args)...);
		}

		return Handle;
	}

private:
	typedef void (*DestroyFnSignature)(uint8* TargetBuffer, int32 NumElem);

	static constexpr int32 DEFAULT_BLOCK_SIZE = 32;

	// structure holding each registered type information
	struct FDataTypeDef
	{
		FParamTypeHandle ParamTypeHandle;
		DestroyFnSignature DestroyTypeFn = nullptr;
		int32 ElementSize = 0;
		int32 ElementAlign = 0;
		int32 AllocationBlockSize = 0;
	};

	struct FReferencePoseData
	{
		FReferencePoseData() = default;
		
		FReferencePoseData(const FDataHandle& InAnimationDataHandle, const FDelegateHandle& InDelegateHandle)
			: AnimationDataHandle(InAnimationDataHandle)
			, DelegateHandle(InDelegateHandle)
		{
		}

		FReferencePoseData(FDataHandle&& InAnimationDataHandle, FDelegateHandle&& InDelegateHandle)
			: AnimationDataHandle(MoveTemp(InAnimationDataHandle))
			, DelegateHandle(MoveTemp(InDelegateHandle))
		{
		}

		FDataHandle AnimationDataHandle;
		FDelegateHandle DelegateHandle;
	};

	// Map holding registered types
	TMap<FParamTypeHandle, FDataTypeDef> DataTypeDefs;
	// Lock for registered types map
	FRWLock DataTypeDefsLock;

	TSet<Private::FAllocatedBlock*> AllocatedBlocks;
	// Lock for allocated blocks
	FRWLock AllocatedBlocksLock;

	// Map holding named data
	TMap <FName, FDataHandle> StoredData;
	mutable FRWLock StoredDataLock;

	// Map holding reference poses for SkeletalMeshes
	TMap <TWeakObjectPtr<USkeletalMeshComponent>, FReferencePoseData> SkeletalMeshReferencePoses;
	mutable FRWLock SkeletalMeshReferencePosesLock;

	// Registers a type and sets the allocation block size
	template<typename DataType>
	FDataTypeDef* RegisterDataType_Impl(int32 AllocationBlockSize)
	{
		FParamTypeHandle ParamTypeHandle = FParamTypeHandle::GetHandle<DataType>();
		check(ParamTypeHandle.IsValid());

		const int32 ElementSize = ParamTypeHandle.GetSize();
		const int32 ElementAlign = ParamTypeHandle.GetAlignment();

		// If we use raw types, I need a per element destructor
		DestroyFnSignature DestroyFn = [](uint8* TargetBuffer, int32 NumElem)->void
		{
			const DataType* Ptr = (DataType*)TargetBuffer;
			for (int i = 0; i < NumElem; i++)
			{
				Ptr[i].~DataType();
			}
		};

		FDataTypeDef* AddedDef = nullptr;
		{
			FRWScopeLock WriteLock(DataTypeDefsLock, SLT_Write);

			AddedDef = &DataTypeDefs.FindOrAdd(ParamTypeHandle, { ParamTypeHandle, DestroyFn, ElementSize, ElementAlign, AllocationBlockSize });
			check(AddedDef->ParamTypeHandle == ParamTypeHandle); // check we have not added two different types with the same ID
		}

		return AddedDef;
	}

	void OnLODRequiredBonesUpdate(USkeletalMeshComponent* SkeletalMeshComponent, int32 LODLevel, const TArray<FBoneIndexType>& LODRequiredBones);
	
	void FreeAllocatedBlock(Private::FAllocatedBlock * AllocatedBlock);

	void ReleaseReferencePoseData();

// --- ---
private:
	friend class FModule;
	friend struct FDataHandle;

	// Initialize the global registry
	static void Init();

	// Shutdown the global registry
	static void Destroy();

	volatile int32 HandleCounter = 0;


private:
	static void HandlePostGarbageCollect();
};

} // namespace UE::AnimNext
