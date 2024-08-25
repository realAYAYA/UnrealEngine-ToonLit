// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "DecoratorBase/DecoratorBinding.h"
#include "DecoratorBase/DecoratorHandle.h"			// Derived types are likely to refer to other decorators as children
#include "DecoratorBase/DecoratorInstanceData.h"
#include "DecoratorBase/DecoratorMode.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/DecoratorSharedData.h"
#include "DecoratorBase/DecoratorUID.h"
#include "DecoratorBase/IDecoratorInterface.h"
#include "DecoratorBase/LatentPropertyHandle.h"

#include <type_traits>

class FArchive;

// Helper macros
// In the decorator class declaration, this macro declares the Super alias and base functions we override
#define DECLARE_ANIM_DECORATOR(DecoratorName, DecoratorNameHash, SuperDecoratorName) \
	using DecoratorSuper = SuperDecoratorName; \
	/* FDecorator impl */ \
	static constexpr UE::AnimNext::FDecoratorUID DecoratorUID = UE::AnimNext::FDecoratorUID(DecoratorNameHash, TEXT(#DecoratorName)); \
	virtual UE::AnimNext::FDecoratorUID GetDecoratorUID() const override { return DecoratorUID; } \
	virtual FString GetDecoratorName() const override { return TEXT(#DecoratorName); } \
	static const UE::AnimNext::FDecoratorMemoryLayout DecoratorMemoryDescription; \
	virtual UE::AnimNext::FDecoratorMemoryLayout GetDecoratorMemoryDescription() const override { return DecoratorMemoryDescription; } \
	virtual UScriptStruct* GetDecoratorSharedDataStruct() const override { return FSharedData::StaticStruct(); } \
	virtual void ConstructDecoratorInstance(const UE::AnimNext::FExecutionContext& Context, const UE::AnimNext::FDecoratorBinding& Binding) const override; \
	virtual void DestructDecoratorInstance(const UE::AnimNext::FExecutionContext& Context, const UE::AnimNext::FDecoratorBinding& Binding) const override; \
	virtual const UE::AnimNext::IDecoratorInterface* GetDecoratorInterface(UE::AnimNext::FDecoratorInterfaceUID InterfaceUID) const override; \
	virtual TConstArrayView<FDecoratorInterfaceUID> GetDecoratorInterfaces() const override; \
	virtual uint32 GetNumLatentDecoratorProperties() const override { return -FSharedData::GetLatentPropertyIndex(~(size_t)0); } \
	virtual FDecoratorLatentPropertyMemoryLayout GetLatentPropertyMemoryLayout(FName PropertyName, uint32 PropertyIndex) const override; \
	static_assert(std::is_base_of<FAnimNextDecoratorSharedData, FSharedData>::value, "Decorator shared data must derive from FAnimNextDecoratorSharedData"); \
	static_assert(std::is_base_of<FDecoratorInstanceData, FInstanceData>::value, "Decorator instance data must derive from FDecoratorInstanceData");

#define DECLARE_ABSTRACT_ANIM_DECORATOR(DecoratorName, DecoratorNameHash, SuperDecoratorName) \
	using DecoratorSuper = SuperDecoratorName; \
	/* FDecorator impl */ \
	static constexpr UE::AnimNext::FDecoratorUID DecoratorUID = UE::AnimNext::FDecoratorUID(DecoratorNameHash, TEXT(#DecoratorName)); \
	virtual UE::AnimNext::FDecoratorUID GetDecoratorUID() const override { return DecoratorUID; } \
	virtual FString GetDecoratorName() const override { return TEXT(#DecoratorName); }

// In the decorator cpp, these three macros implement the base functionality
// 
// Usage is as follow:
// #define DECORATOR_INTERFACE_ENUMERATOR(GeneratorMacro) \
//		GeneratorMacro(IHierarchy) \
//		GeneratorMacro(IUpdate) \
//
// GENERATE_ANIM_DECORATOR_IMPLEMENTATION(FMyDecorator, DECORATOR_INTERFACE_ENUMERATOR)
// #undef DECORATOR_INTERFACE_ENUMERATOR

// Implements various parts of FDecorator
#define ANIM_NEXT_IMPL_DEFINE_ANIM_DECORATOR(DecoratorName) \
	const UE::AnimNext::FDecoratorMemoryLayout DecoratorName::DecoratorMemoryDescription = \
		UE::AnimNext::FDecoratorMemoryLayout{ sizeof(DecoratorName), alignof(DecoratorName), sizeof(DecoratorName::FSharedData), alignof(DecoratorName::FSharedData), sizeof(DecoratorName::FInstanceData), alignof(DecoratorName::FInstanceData) }; \
	FDecoratorLatentPropertyMemoryLayout DecoratorName::GetLatentPropertyMemoryLayout(FName PropertyName, uint32 PropertyIndex) const \
	{ \
		/* Thread safe cache initialization */ \
		static TArray<FDecoratorLatentPropertyMemoryLayout> CachedLatentPropertyMemoryLayouts = [this](){ TArray<FDecoratorLatentPropertyMemoryLayout> Result; Result.SetNum(GetNumLatentDecoratorProperties()); return Result; }(); \
		return GetLatentPropertyMemoryLayoutImpl(PropertyName, PropertyIndex, CachedLatentPropertyMemoryLayouts); \
	} \
	void DecoratorName::ConstructDecoratorInstance(const UE::AnimNext::FExecutionContext& Context, const UE::AnimNext::FDecoratorBinding& Binding) const \
	{ \
		FInstanceData* Data = new(Binding.GetInstanceData<FInstanceData>()) FInstanceData(); \
		Data->Construct(Context, Binding); \
	} \
	void DecoratorName::DestructDecoratorInstance(const UE::AnimNext::FExecutionContext& Context, const UE::AnimNext::FDecoratorBinding& Binding) const \
	{ \
		FInstanceData* Data = Binding.GetInstanceData<FInstanceData>(); \
		Data->Destruct(Context, Binding); \
		Data->~FInstanceData(); \
	}

// Helper that handles the GetDecoratorInterface() details for each interface specified by the generator macro
#define ANIM_NEXT_IMPL_GET_INTERFACE_IMPL_FOR_INTERFACE(InterfaceName) \
	if (InInterfaceUID == InterfaceName::InterfaceUID) \
	{ \
		return static_cast<const InterfaceName*>(this); \
	}

// Implements GetDecoratorInterface()
#define ANIM_NEXT_IMPL_DEFINE_ANIM_DECORATOR_GET_INTERFACE(DecoratorName, InterfaceEnumeratorMacro) \
	const UE::AnimNext::IDecoratorInterface* DecoratorName::GetDecoratorInterface(UE::AnimNext::FDecoratorInterfaceUID InInterfaceUID) const \
	{ \
		InterfaceEnumeratorMacro(ANIM_NEXT_IMPL_GET_INTERFACE_IMPL_FOR_INTERFACE) \
		/* Forward to base implementation */ \
		return DecoratorSuper::GetDecoratorInterface(InInterfaceUID); \
	}

// Helper that handles the GetDecoratorInterfaces() details for each interface specified by the generator macro
#define ANIM_NEXT_IMPL_GET_INTERFACES_IMPL_FOR_INTERFACE(InterfaceName) InterfaceName::InterfaceUID,

// Implements GetDecoratorInterfaces()
#define ANIM_NEXT_IMPL_DEFINE_ANIM_DECORATOR_GET_INTERFACES(DecoratorName, InterfaceEnumeratorMacro) \
	TConstArrayView<UE::AnimNext::FDecoratorInterfaceUID> DecoratorName::GetDecoratorInterfaces() const \
	{ \
		/* Thread safe cache initialization */ \
		static TArray<UE::AnimNext::FDecoratorInterfaceUID> CachedInterfaceList = FDecorator::BuildDecoratorInterfaceList( \
		DecoratorSuper::GetDecoratorInterfaces(), \
		{ \
			InterfaceEnumeratorMacro(ANIM_NEXT_IMPL_GET_INTERFACES_IMPL_FOR_INTERFACE) \
		}); \
		return CachedInterfaceList; \
	}

/**
  * This macro defines the necessary boilerplate for implementing FDecorator. See above for usage example.
  */
#define GENERATE_ANIM_DECORATOR_IMPLEMENTATION(DecoratorName, InterfaceEnumeratorMacro) \
	ANIM_NEXT_IMPL_DEFINE_ANIM_DECORATOR(DecoratorName) \
	ANIM_NEXT_IMPL_DEFINE_ANIM_DECORATOR_GET_INTERFACE(DecoratorName, InterfaceEnumeratorMacro) \
	ANIM_NEXT_IMPL_DEFINE_ANIM_DECORATOR_GET_INTERFACES(DecoratorName, InterfaceEnumeratorMacro)

// Allows a decorator to auto-register and unregister within the current execution scope
// The decorator must be found in the current scope without a namespace qualification
#define AUTO_REGISTER_ANIM_DECORATOR(DecoratorName) \
	UE::AnimNext::FDecoratorStaticInitHook DecoratorName##Hook( \
		[](void* DestPtr, UE::AnimNext::FDecoratorMemoryLayout& MemoryDesc) -> UE::AnimNext::FDecorator* \
		{ \
			MemoryDesc = DecoratorName::DecoratorMemoryDescription; \
			return DestPtr != nullptr ? new(DestPtr) DecoratorName() : nullptr; \
		});

namespace UE::AnimNext
{
	struct FDecorator;
	struct FDecoratorMemoryLayout;
	class FDecoratorReader;
	class FDecoratorWriter;
	struct FExecutionContext;

	// A function pointer to a shim to construct a decorator into the desired memory location
	// When called with a nullptr DestPtr, the function returns nullptr and only populates the
	// memory description output argument. This allows the caller to determine how much space
	// to reserve and how to properly align it. This is similar in spirit to various Windows SDK functions.
	using DecoratorConstructorFunc = FDecorator* (*)(void* DestPtr, FDecoratorMemoryLayout& MemoryDesc);

	/**
	 * FDecoratorMemoryLayout
	 * 
	 * Encapsulates size/alignment details for a decorator.
	 */
	struct FDecoratorMemoryLayout
	{
		// The size in bytes of an instance of the decorator class which derives from FDecorator
		uint32 DecoratorSize = 0;

		// The alignment in bytes of an instance of the decorator class which derives from FDecorator
		uint32 DecoratorAlignment = 1;

		// The size in bytes of the shared data for the decorator which derives from FAnimNextDecoratorSharedData
		uint32 SharedDataSize = 0;

		// The alignment in bytes of the shared data for the decorator which derives from FAnimNextDecoratorSharedData
		uint32 SharedDataAlignment = 1;

		// The size in bytes of the instance data for the decorator which derives from FDecoratorInstanceData
		uint32 InstanceDataSize = 0;

		// The alignment in bytes of the instance data for the decorator which derives from FDecoratorInstanceData
		uint32 InstanceDataAlignment = 1;
	};

	/**
	 * FDecoratorLatentPropertyMemoryLayout
	 *
	 * Encapsulates size/alignment details for a latent property.
	 */
	struct FDecoratorLatentPropertyMemoryLayout
	{
		// The size in bytes of the latent property
		uint32 Size = 0;

		// The alignment in bytes of the latent property
		uint32 Alignment = 1;
	};

	/**
	 * FDecorator
	 * 
	 * Base class for all decorators.
	 * A decorator can implement any number of interfaces based on IDecoratorInterface.
	 * A decorator may derive from another decorator.
	 * A decorator should implement GetInterface(..) and test against the interfaces that it supports.
	 * 
	 * Decorators should NOT have any internal state, hence why all API functions are 'const'.
	 * The reason for this is that at runtime, a single instance of every decorator exists.
	 * That single instance is used by all instances of a decorator on a node and concurrently
	 * on all worker threads.
	 * 
	 * Decorators can have shared read-only data that all instances of a graph can use (e.g. hard-coded properties).
	 * Shared data must derive from FAnimNextDecoratorSharedData.
	 * Decorators can have instance data (e.g. blend weight).
	 * Instance data must derive from FDecoratorInstanceData.
	 */
	struct ANIMNEXT_API FDecorator
	{
		virtual ~FDecorator() {}

		// Empty shared/instance data types
		// Derived types must define an alias for these
		using FSharedData = FAnimNextDecoratorSharedData;
		using FInstanceData = FDecoratorInstanceData;

		// The globally unique UID for this decorator
		// Derived types will have their own DecoratorUID member that hides/aliases/shadows this one
		// @see DECLARE_ANIM_DECORATOR
		static constexpr FDecoratorUID DecoratorUID = FDecoratorUID(0x4fc735a2, TEXT("FDecorator"));

		// Returns the globally unique UID for this decorator
		virtual FDecoratorUID GetDecoratorUID() const { return DecoratorUID; };

		// Returns the decorator name
		virtual FString GetDecoratorName() const { return TEXT("FDecorator"); }

		// Returns the memory requirements of the derived decorator instance
		virtual FDecoratorMemoryLayout GetDecoratorMemoryDescription() const = 0;

		// Returns the UScriptStruct associated with the shared data for the decorator
		virtual UScriptStruct* GetDecoratorSharedDataStruct() const { return FSharedData::StaticStruct(); }

		// Called when a new instance of the decorator is created or destroyed
		// Derived types must override this and forward to the instance data constructor/destructor
		virtual void ConstructDecoratorInstance(const FExecutionContext& Context, const FDecoratorBinding& Binding) const = 0;
		virtual void DestructDecoratorInstance(const FExecutionContext& Context, const FDecoratorBinding& Binding) const = 0;

		// Returns the decorator mode.
		virtual EDecoratorMode GetDecoratorMode() const = 0;

		// Returns a pointer to the specified interface if it is supported.
		// Derived types must override this.
		virtual const IDecoratorInterface* GetDecoratorInterface(FDecoratorInterfaceUID InterfaceUID) const
		{
			// TODO:
			// if/else sequence with static_casts to get the right v-table
			// could be implemented with two tables: one of UIDs, another with matching offsets to 'this'
			// we could scan the first table with SIMD, 4x UIDs at a time with 'cmpeq' to generate a mask
			// we can mode the mask into a general register, if non-zero, we have a match
			// using the mask, we can easily compute the UID offset in our 4x entry by counting leading/trailing zeroes
			// using the UID offset, we can load and apply the correct offset
			// may or may not be faster, but it shifts the burden from code cache to data cache and we can better control locality
			// we could store the tables contiguous with one another, offsets could be 16 bit or maybe 8 bit (multiple of pointer size)
			// we could store the tables contiguous with the tables of other decorators for better cache locality
			// by using tables, it means the lookup code can live in a single place and remain hot
			// it means we can test 4x UIDs at a time, or interleave and test 8x or 16x
			// it means we can quickly early out if none of the interfaces match (common case?) since we don't need to test
			// all of them one by one
			// SIMD code path also opens the door for cheap bulk interface queries where we query up to 4x interface UIDs and
			// return 4x interface offsets (caller can generate pointers easily)

			// Base class doesn't implement any interfaces
			// Derived types must implement this
			return nullptr;
		}

		// Returns a list of interfaces that this decorator supports
		virtual TConstArrayView<FDecoratorInterfaceUID> GetDecoratorInterfaces() const { return TConstArrayView<FDecoratorInterfaceUID>(); }

		// The number of latent property properties in the shared data of this decorator
		virtual uint32 GetNumLatentDecoratorProperties() const { return 0; }

		// Returns the memory layout of the specified latent property
		virtual FDecoratorLatentPropertyMemoryLayout GetLatentPropertyMemoryLayout(FName PropertyName, uint32 PropertyIndex) const { return FDecoratorLatentPropertyMemoryLayout(); }

		// Called to serialize decorator shared data
		virtual void SerializeDecoratorSharedData(FArchive& Ar, FAnimNextDecoratorSharedData& SharedData) const;

#if WITH_EDITOR
		// Takes the editor properties as authored in the graph and converts them into an instance of the FAnimNextDecoratorSharedData
		// derived type using UE reflection.
		// Decorators can override this function to control how editor only properties are coerced into the runtime shared data
		// instance.
		virtual void SaveDecoratorSharedData(const TFunction<FString(FName PropertyName)>& GetDecoratorProperty, FAnimNextDecoratorSharedData& OutSharedData) const;

		// Takes the editor properties as authored in the graph and returns the latent property metadata using UE reflection.
		TArray<FLatentPropertyMetadata> GetLatentPropertyHandles(
			bool bFilterEditorOnly,
			const TFunction<uint16(FName PropertyName)>& GetDecoratorLatentPropertyIndex) const;
#endif

	protected:
		// Implements GetLatentPropertyMemoryLayout() by allowing a map to be provided for caching purposes to speed up repeated queries
		FDecoratorLatentPropertyMemoryLayout GetLatentPropertyMemoryLayoutImpl(
			FName PropertyName,
			uint32 PropertyIndex,
			TArray<FDecoratorLatentPropertyMemoryLayout>& LatentPropertyMemoryLayouts) const;

		// Builds a list of interfaces with the provided super interfaces and current interfaces as an initializer list
		static TArray<FDecoratorInterfaceUID> BuildDecoratorInterfaceList(
			const TConstArrayView<FDecoratorInterfaceUID>& SuperInterfaces,
			std::initializer_list<FDecoratorInterfaceUID> InterfaceList);
	};

	// Base class for base decorators that are standalone
	struct ANIMNEXT_API FBaseDecorator : FDecorator
	{
		DECLARE_ABSTRACT_ANIM_DECORATOR(FBaseDecorator, 0xd23dcf79, FDecorator)

		virtual EDecoratorMode GetDecoratorMode() const override { return EDecoratorMode::Base; }
	};

	// Base class for additive decorators that override behavior of other decorators
	struct ANIMNEXT_API FAdditiveDecorator : FDecorator
	{
		DECLARE_ABSTRACT_ANIM_DECORATOR(FAdditiveDecorator, 0x7ab3732a, FDecorator)

		virtual EDecoratorMode GetDecoratorMode() const override { return EDecoratorMode::Additive; }
	};

	/**
	 * FDecoratorStaticInitHook
	 *
	 * Allows decorators to automatically register/unregister within the current scope.
	 * This can be used during static init.
	 */
	struct ANIMNEXT_API FDecoratorStaticInitHook final
	{
		explicit FDecoratorStaticInitHook(DecoratorConstructorFunc InDecoratorConstructor);
		~FDecoratorStaticInitHook();

	private:
		DecoratorConstructorFunc DecoratorConstructor;
	};
}
