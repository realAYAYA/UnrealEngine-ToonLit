// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "UObject/NameTypes.h"
#include "Containers/ContainersFwd.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/PagedArray.h"
#include "Param/ParamId.h"
#include "Param/ParamTypeHandle.h"
#include "Param/ParamCompatibility.h"
#include "UObject/GCObject.h"
#include "Param/ParamStackLayerHandle.h"
#include "ParamEntry.h"
#include "Param/ParamResult.h"
#include "AnimNextStats.h"
#include "Containers/HashTable.h"

class UAnimNextSchedule;
class UAnimNextSchedulerWorldSubsystem;
struct FInstancedPropertyBag;
struct FAnimSubsystem_AnimNextParameters;
class FAnimationAnimNextRuntimeTest_GraphExecute;
class FAnimationAnimNextRuntimeTest_GraphExecuteLatent;
struct FAnimNextSchedulerEntry;

namespace UE::AnimNext
{
	struct FScheduleContext;
	struct FSchedulerImpl;
	struct FUObjectLayer;
	struct FInstancedPropertyBagLayer;
	struct FInstancedPropertyBagValueLayer;
	struct FAnimGraphParamStackScope;
	struct FScheduleInstanceData;
	struct FScheduleTickFunction;
	struct FParamStackLayer;
}

namespace UE::AnimNext::Tests
{
	class FParamStackTest;
}

namespace UE::AnimNext
{

// Stack of parameter layers.
// Acts as an associative container - allows retrieval of parameter values (by ID) that have been
// pushed onto the stack in 'layers'.
// Parameter values (and notably types) on higher stack layers override those on lower layers. 
// Older values and types are restored when a layer is popped.
// Parameter values, once pushed, can be overridden by subsequent layers, but they cannot be 
// removed until the layer that initially introduces the parameter is popped.
struct FParamStack
{
	friend class Tests::FParamStackTest;
	friend struct FScheduleContext;
	friend struct FScheduler;
	friend struct FSchedulerImpl;
	friend class ::UAnimNextSchedule;
	friend struct ::FAnimSubsystem_AnimNextParameters;
	friend struct FParamStackLayer;
	friend struct FUObjectLayer;
	friend struct FInstancedPropertyBagLayer;
	friend struct FInstancedPropertyBagValueLayer;
	friend struct FAnimGraphParamStackScope;
	friend struct FScheduleInstanceData;
	friend class ::UAnimNextSchedulerWorldSubsystem;
	friend struct FScheduleTickFunction;
	friend struct ::FAnimNextSchedulerEntry;
	friend class ::FAnimationAnimNextRuntimeTest_GraphExecute;
	friend class ::FAnimationAnimNextRuntimeTest_GraphExecuteLatent;
	friend class ::UAnimNextComponent;

	template<typename AllocatorType> 
	friend struct TLayerBuilder;

	template<typename T, ESPMode>
	friend class SharedPointerInternals::TIntrusiveReferenceController;
	
private:
	// A layer held on the stack, may be owned by the stack or not
	struct FPushedLayer
	{
		FPushedLayer(FParamStackLayer& InLayer, FParamStack& InStack);

		FParamStackLayer& Layer;

		// Serial number used for identifying layers to pop
		uint32 SerialNumber = 0;
	};

public:
	// Opaque handle to a layer on the stack
	struct FPushedLayerHandle
	{
		FPushedLayerHandle() = default;

		bool IsValid() const
		{
			return Index != MAX_uint32 && SerialNumber != 0;
		}

		void Invalidate()
		{
			Index = MAX_uint32;
			SerialNumber = 0;
		}

	private:
		friend struct FParamStack;

		FPushedLayerHandle(uint32 InIndex, uint32 InSerialNumber)
			: Index(InIndex)
			, SerialNumber(InSerialNumber)
		{}

		// Index into the stack
		uint32 Index = MAX_uint32;

		// Serial number of the layer
		uint32 SerialNumber = 0;
	};

	// Builder struct to help build combined layers of parameters
	template<typename AllocatorType = FDefaultAllocator>
	struct TLayerBuilder
	{
		TLayerBuilder(int32 InNumParams = 0)
		{
			Params.Reserve(InNumParams);
		}

		template<typename ValueType>
		void Add(FName InName, ValueType&& InValue)
		{
			Params.Emplace(FParamId(InName),
				Private::FParamEntry(
					FParamTypeHandle::GetHandle<std::remove_reference_t<ValueType>>(),
					TArrayView<uint8>(const_cast<uint8*>(reinterpret_cast<const uint8*>(&InValue)), sizeof(std::remove_reference_t<ValueType>)),
					std::is_reference_v<ValueType>,
					!std::is_const_v<std::remove_reference_t<ValueType>>
				)
			);
		}

		FParamStackLayerHandle MakeLayer()
		{
			return FParamStack::Get().MakeLayer(Params);
		}

	private:
		TArray<TPair<FParamId, Private::FParamEntry>, AllocatorType> Params;
	};

	using FLayerBuilder = TLayerBuilder<>;

	ANIMNEXT_API ~FParamStack();

	// Get the param stack for this thread
	static ANIMNEXT_API FParamStack& Get();

	// Push a value as a parameter layer
	// @param	InParamId			Parameter ID for the parameter
	// @param	InValue				Value to push
	template<typename... ValueType>
	FPushedLayerHandle PushValue(FParamId InParamId, ValueType&&... InValue)
	{
		return PushValues(InParamId, Forward<ValueType>(InValue)...);
	}

	// Push a value as a parameter layer
	// @param	InParamId			Parameter name for the parameter
	// @param	InValue				Value to push
	template<typename... ValueType>
	FPushedLayerHandle PushValue(FName InName, ValueType&&... InValue)
	{
		return PushValues(FParamId(InName), Forward<ValueType>(InValue)...);
	}

	// Push an interleaved parameter list of keys and values as a parameter layer
	// @param	InValues	Values to push
	template<typename... Args>
	FPushedLayerHandle PushValues(Args&&... InValues)
	{
		constexpr int32 NumItems = sizeof...(InValues) / 2;
		TArray<Private::FParamEntry, TInlineAllocator<NumItems>> ParamIdValues;
		ParamIdValues.Reserve(NumItems);
		return PushValuesHelper(ParamIdValues, Forward<Args>(InValues)...);
	}

	// Push a layer
	ANIMNEXT_API FPushedLayerHandle PushLayer(const FParamStackLayerHandle& InLayerHandle);

	// Create a cached parameter layer from an instanced property bag. This layer will own a copy of the property bag used to represent the layer.
	static ANIMNEXT_API FParamStackLayerHandle MakeValueLayer(const FInstancedPropertyBag& InInstancedPropertyBag);

	// Create a cached parameter layer from an instanced property bag. This layer will reference the supplied property bag and does not transfer ownership.
	static ANIMNEXT_API FParamStackLayerHandle MakeReferenceLayer(FInstancedPropertyBag& InInstancedPropertyBag);

	// Create a cached parameter layer by remapping the entries from another layer
	// Original layer continues to own the memory
	static ANIMNEXT_API FParamStackLayerHandle MakeRemappedLayer(const FParamStackLayerHandle& InLayer, const TMap<FName, FName>& InMapping);

	// Make a parameter layer from a value
	// @param	InParamId			Parameter ID for the parameter
	// @param	InValue				Value to push
	template<typename... ValueType>
	static FParamStackLayerHandle MakeValueLayer(FParamId InParamId, ValueType&&... InValue)
	{
		return MakeValuesLayer(InParamId, Forward<ValueType>(InValue)...);
	}

	// Make a parameter layer from a value
	// @param	InName				Parameter name for the parameter
	// @param	InValue				Value to push
	template<typename... ValueType>
	static FParamStackLayerHandle MakeValueLayer(FName InName, ValueType&&... InValue)
	{
		return MakeValuesLayer(FParamId(InName), Forward<ValueType>(InValue)...);
	}

	// Make a parameter layer from an interleaved parameter list of keys and values
	// @param	InValues	Values to push
	template<typename... Args>
	static FParamStackLayerHandle MakeValuesLayer(Args&&... InValues)
	{
		constexpr int32 NumItems = sizeof...(InValues) / 2;
		TArray<Private::FParamEntry, TInlineAllocator<NumItems>> ParamIdValues;
		ParamIdValues.Reserve(NumItems);
		return MakeValuesLayerHelper(ParamIdValues, Forward<Args>(InValues)...);
	}

	// Pop a parameter layer. Attempting to pop an invalid layer handle is supported.
	// Asserts if the layer supplied is valid and not the top layer
	ANIMNEXT_API void PopLayer(FPushedLayerHandle InLayer);

	// Get a pointer to a parameter's value given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to aresult that will be filled if an error occurs
	// @return nullptr if the parameter is not mapped
	template<typename ValueType>
	const ValueType* GetParamPtr(FParamId InParamId, FParamResult* OutResult = nullptr) const
	{
		TConstArrayView<uint8> Data;
		FParamTypeHandle TypeHandle;
		FParamResult Result = GetParamData(InParamId, FParamTypeHandle::GetHandle<ValueType>(), Data, TypeHandle);
		if (OutResult)
		{
			*OutResult = Result;
		}
		return reinterpret_cast<const ValueType*>(Data.GetData());
	}

	// Get a pointer to a parameter's mutable value given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return nullptr if the parameter is not mapped or mutable
	template<typename ValueType>
	ValueType* GetMutableParamPtr(FParamId InParamId, FParamResult* OutResult = nullptr)
	{
		TArrayView<uint8> Data;
		FParamTypeHandle TypeHandle;
		FParamResult Result = GetMutableParamData(InParamId, FParamTypeHandle::GetHandle<ValueType>(), Data, TypeHandle);
		if (OutResult)
		{
			*OutResult = Result;
		}
		return reinterpret_cast<ValueType*>(Data.GetData());
	}

	// Get a pointer to a parameter's value given a FName.
	// Using the FParamId overload will avoid having to hash the FName to recover the ID.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return nullptr if the parameter is not mapped
	template<typename ValueType>
	const ValueType* GetParamPtr(FName InKey, FParamResult* OutResult = nullptr) const
	{
		return GetParamPtr<ValueType>(FParamId(InKey), OutResult);
	}

	// Get a pointer to a parameter's mutable value given a FName.
	// Using the FParamId overload will avoid having to hash the FName to recover the ID.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return nullptr if the parameter is not mapped
	template<typename ValueType>
	ValueType* GetMutableParamPtr(FName InKey, FParamResult* OutResult = nullptr)
	{
		return GetMutableParamPtr<ValueType>(FParamId(InKey), OutResult);
	}

	// Get the value of a parameter given a FParamId. Potentially performs a copy of the value.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return the parameter's value, or a default value if the parameter is not present
	template<typename ValueType>
	ValueType GetParamValue(FParamId InParamId, FParamResult* OutResult = nullptr) const
	{
		if (const ValueType* ParamPtr = GetParamPtr<ValueType>(InParamId, OutResult))
		{
			return *ParamPtr;
		}
		return ValueType();
	}

	// Get the value of a parameter given a FName. Potentially performs a copy of the value.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return the parameter's value, or a default value if the parameter is not present
	template<typename ValueType>
	ValueType GetParamValue(FName InKey, FParamResult* OutResult = nullptr) const
	{
		if (const ValueType* ParamPtr = GetParamPtr<ValueType>(FParamId(InKey), OutResult))
		{
			return *ParamPtr;
		}
		return ValueType();
	}

	// Get a const reference to the value of a parameter given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return a const reference to the parameter's value. Function asserts if the parameter is not present
	template<typename ValueType>
	const ValueType& GetParam(FParamId InParamId, FParamResult* OutResult = nullptr) const
	{
		const ValueType* ParamPtr = GetParamPtr<ValueType>(InParamId, OutResult);
		check(ParamPtr);
		return *ParamPtr;
	}

	// Get a const reference to the value of a parameter given a FName.
	// Using the FParamId overload will avoid having to hash the FName to recover the ID.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return a const reference to the parameter's value. Function asserts if the parameter is not present
	template<typename ValueType>
	const ValueType& GetParam(FName InKey, FParamResult* OutResult = nullptr) const
	{
		const ValueType* ParamPtr = GetParamPtr<ValueType>(FParamId(InKey), OutResult);
		check(ParamPtr);
		return *ParamPtr;
	}

	// Get a mutable reference to the value of a parameter given a FParamId.
	// @param	InParamId			Parameter ID to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return a mutable reference to the parameter's value. Function asserts if the parameter is not present
	template<typename ValueType>
	ValueType& GetMutableParam(FParamId InParamId, FParamResult* OutResult = nullptr)
	{
		ValueType* ParamPtr = GetMutableParamPtr<ValueType>(InParamId, OutResult);
		check(ParamPtr);
		return *ParamPtr;
	}

	// Get a mutable reference to the value of a parameter given a FName.
	// Using the FParamId overload will avoid having to hash the FName to recover the ID.
	// @param	InKey				Parameter name to find the currently-pushed value for
	// @param	OutResult			Optional ptr to a result that will be filled if an error occurs
	// @return a mutable reference to the parameter's value. Function asserts if the parameter is not present
	template<typename ValueType>
	ValueType& GetMutableParam(FName InKey, FParamResult* OutResult = nullptr)
	{
		ValueType* ParamPtr = GetMutableParamPtr<ValueType>(FParamId(InKey), OutResult);
		check(ParamPtr);
		return *ParamPtr;
	}

	// Get a parameter's data given an FParamId.
	// @param	InParamId				Parameter ID to find the currently-pushed value for
	// @param	InParamTypeHandle		Handle to the type of the parameter, which must match the stored type for a value to
	//									be returned according to the supplied compatibility
	// @param	OutParamData			View into the parameters data that will be filled
	// @param	OutParamTypeHandle		Handle to the type of the parameter if the parameter is present in the stack.
	//									This will be valid even if the types are deemed incompatible.
	// @param	InRequiredCompatibility	The minimum required compatibility level
	// @return an enum describing the result of the operation @see FParamResult
	ANIMNEXT_API FParamResult GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal()) const;
	ANIMNEXT_API FParamResult GetParamData(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal()) const;

	// Get a parameter's mutable data given an FParamId.
	// @param	InParamId				Parameter ID to find the currently-pushed value for
	// @param	InParamTypeHandle		Handle to the type of the parameter, which must match the stored type for a value to
	//									be returned according to the supplied compatibility
	// @param	OutParamData			View into the parameters data that will be filled
	// @param	OutParamTypeHandle		Handle to the type of the parameter if the parameter is present in the stack.
	//									This will be valid even if the types are deemed incompatible.
	// @param	InRequiredCompatibility	The minimum required compatibility level
	// @return an enum describing the result of the operation @see FParamResult
	ANIMNEXT_API FParamResult GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal());
	ANIMNEXT_API FParamResult GetMutableParamData(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility = FParamCompatibility::Equal());

private:
	ANIMNEXT_API FParamStack();

	ANIMNEXT_API void SetParent(TWeakPtr<const FParamStack> InParent);

	// Get the param stack that is attached to the current thread
	static ANIMNEXT_API TWeakPtr<FParamStack> GetForCurrentThread();

	// Behavior flag for coalescing parent stacks on thread attachment 
	enum class ECoalesce
	{
		// Do nothing
		None,

		// Coalesce parent stacks into this stack
		Coalesce
	};

	// Attach a param stack to the current thread
	static ANIMNEXT_API void AttachToCurrentThread(TWeakPtr<FParamStack> InStack, ECoalesce InCoalesce = ECoalesce::None);

	// Attach a param stack that is 'pending' for the specified object to the current thread.
	// @return true if the stack was successfully attached
	static ANIMNEXT_API bool AttachToCurrentThreadForPendingObject(const UObject* InObject, ECoalesce InCoalesce = ECoalesce::None);

	// Behavior flag for de-coalescing parent stacks on thread detachment
	enum class EDecoalesce
	{
		// Do nothing
		None,

		// Remove coalesced parent stacks from this stack
		Decoalesce
	};

	// Detach a param stack to the current thread
	static ANIMNEXT_API TWeakPtr<FParamStack> DetachFromCurrentThread(EDecoalesce InDecoalesce = EDecoalesce::None);
	
	// Detach a param stack that is 'pending' for the specified object from the current thread.
	// @return true if the stack was successfully detached
	static ANIMNEXT_API bool DetachFromCurrentThreadForPendingObject(const UObject* InObject, EDecoalesce InDecoalesce = EDecoalesce::None);

	// Adds a stack asscociated with a 'pending' object - that is, an object that is due to have some
	// asscociated logic run that needs to access the relevant stack.
	// If the supplied stack is null, a new stack will be added, parented to the global stack.
	static ANIMNEXT_API void AddForPendingObject(const UObject* InObject, TWeakPtr<FParamStack> InStack = nullptr);

	// Removes a stack asscociated with a 'pending' object. Asserts if any existing stack still has outstanding references.
	static ANIMNEXT_API void RemoveForPendingObject(const UObject* InObject);

	// Create a cached parameter layer from set of params. Mutabilty is on a per-parameter basis.
	static ANIMNEXT_API FParamStackLayerHandle MakeLayer(TConstArrayView<Private::FParamEntry> InParams);

	// Push an internally-owned layer. Copies parameter data to internal storage.
	ANIMNEXT_API FPushedLayerHandle PushLayer(TConstArrayView<Private::FParamEntry> InParams);

	// Recursive helper function for PushValues
	template <uint32 NumItems, typename FirstType, typename SecondType, typename... OtherTypes>
	FPushedLayerHandle PushValuesHelper(TArray<Private::FParamEntry, TInlineAllocator<NumItems>>& InArray, FirstType&& InFirst, SecondType&& InSecond, OtherTypes&&... InOthers)
	{
		InArray.Emplace(
			Private::FParamEntry(
				FParamId(InFirst),
				FParamTypeHandle::GetHandle<std::remove_reference_t<SecondType>>(),
				TArrayView<uint8>(const_cast<uint8*>(reinterpret_cast<const uint8*>(&InSecond)), sizeof(std::remove_reference_t<SecondType>)),
				std::is_reference_v<SecondType>,
				!std::is_const_v<std::remove_reference_t<SecondType>>
			)
		);

		if constexpr (sizeof...(InOthers) > 0)
		{
			return PushValuesHelper(InArray, Forward<OtherTypes>(InOthers)...);
		}
		else
		{
			return PushLayer(InArray);
		}
	}

	// Recursive helper function for MakeValuesLayer
	template <uint32 NumItems, typename FirstType, typename SecondType, typename... OtherTypes>
	static FParamStackLayerHandle MakeValuesLayerHelper(TArray<Private::FParamEntry, TInlineAllocator<NumItems>>& InArray, FirstType&& InFirst, SecondType&& InSecond, OtherTypes&&... InOthers)
	{
		InArray.Emplace( 
			Private::FParamEntry(
				FParamId(InFirst),
				FParamTypeHandle::GetHandle<std::remove_reference_t<SecondType>>(),
				TArrayView<uint8>(const_cast<uint8*>(reinterpret_cast<const uint8*>(&InSecond)), sizeof(std::remove_reference_t<SecondType>)),
				std::is_reference_v<SecondType>,
				!std::is_const_v<std::remove_reference_t<SecondType>>
			)
		);

		if constexpr (sizeof...(InOthers) > 0)
		{
			return MakeValuesLayerHelper(InArray, Forward<OtherTypes>(InOthers)...);
		}
		else
		{
			return MakeLayer(InArray);
		}
	}

	// Helper function for layer pushes
	FPushedLayerHandle PushLayerInternal(FParamStackLayer& InLayer);

	// Internal helper function for GetParamData
	FParamResult GetParamDataInternal(FParamId InId, FParamTypeHandle InTypeHandle, TConstArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility) const;

	// Internal helper function for GetMutableParamData
	FParamResult GetMutableParamDataInternal(FParamId InId, FParamTypeHandle InTypeHandle, TArrayView<uint8>& OutParamData, FParamTypeHandle& OutParamTypeHandle, FParamCompatibility InRequiredCompatibility);

	// Check whether a given parameter is mutable
	ANIMNEXT_API bool IsMutableParam(FParamId InId) const;

	// Check whether a given parameter is mutable
	bool IsMutableParam(FName InKey) const
	{
		return IsMutableParam(FParamId(InKey));
	}

	// Check whether a given parameter is a reference
	ANIMNEXT_API bool IsReferenceParam(FParamId InId) const;

	// Check whether a given parameter is a reference
	bool IsReferenceParam(FName InKey) const
	{
		return IsReferenceParam(FParamId(InKey));
	}

	// Check the layer to see if it contains the supplied param.
	static bool LayerContainsParam(const FParamStackLayerHandle& InHandle, FName InKey);

	// Allocates any necessary owned storage for non-reference, non-embeded parameters that a layer holds
	// @return the base offset of the storage area
	uint32 AllocAndCopyOwnedParamStorage(TArrayView<Private::FParamEntry> InParams);

	// Frees any owned storage, restores owned storage offset
	void FreeOwnedParamStorage(uint32 InOffset);

	// Get a new serial number for a pushed layer
	uint32 MakeSerialNumber();

	// Find the topmost value for the specified parameter
	const Private::FParamEntry* FindParam(FParamId InId) const;
	Private::FParamEntry* FindMutableParam(FParamId InId);

	// Combine this stack with its parents to allow a single hash query to discover parameters
	// Requires the stack is empty before this is called
	ANIMNEXT_API void Coalesce();

	// Remove all combined parameters that were added in Coalesce()
	ANIMNEXT_API void Decoalesce();

	// Layer stack
	TArray<FPushedLayer> Layers;

	// Stack of entry ptrs, pointing to Layers, indexed by LayerHash
	TArray<Private::FParamEntry*> EntryStack;

	// Hash table for the current layer index per-parameter. Indexes EntryStack.
	FHashTable LayerHash;

	// Owned layers, paged for stable addresses as layers reference them by ptr
	// Grows with each new pushed owned layer
	TPagedArray<FParamStackLayer, 4096> OwnedStackLayers;

	// Parameter storage for owned layers, paged for stable addresses as layer parameters reference them by ptr
	// Grows with each new pushed layer, free'd when the param stack is destroyed
	TPagedArray<uint8, 4096> OwnedLayerParamStorage;

	// Parent stack from outer scope
	TWeakPtr<const FParamStack> WeakParentStack;

	// Coalesced layer handles
	TArray<FPushedLayerHandle> CoalesceLayerHandles;

	// Serial number for stack layers
	uint32 SerialNumber = 0;

	// Flag to indicate coalesced query path
	bool bIsCoalesced = false;
};

}
