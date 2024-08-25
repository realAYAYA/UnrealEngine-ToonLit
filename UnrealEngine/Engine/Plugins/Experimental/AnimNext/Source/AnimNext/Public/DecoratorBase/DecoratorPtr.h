// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::AnimNext
{
	struct FNodeInstance;
	struct FWeakDecoratorPtr;

	/**
	 * Decorator Pointer
	 * A decorator pointer represents a shared pointer to allocated instance data.
	 * It manages reference counting.
	 * It points to a FDecoratorInstanceData or FNodeInstance when resolved.
	 *
	 * A node pointer can also be weak, meaning that it does not update reference counting.
	 * Note that weak pointers will point to garbage if the decorator instance is destroyed.
	 *
	 * Weak decorator pointers should be used carefully!
	 * 
	 * @see FNodeInstance, FDecoratorInstanceData
	 */
	struct ANIMNEXT_API FDecoratorPtr
	{
		// Constructs an invalid pointer handle
		constexpr FDecoratorPtr() noexcept = default;

		// Constructs a pointer handle to the provided instance
		FDecoratorPtr(FNodeInstance* InNodeInstance, uint32 InDecoratorIndex);

		FDecoratorPtr(const FDecoratorPtr& DecoratorPtr);
		FDecoratorPtr(FDecoratorPtr&& DecoratorPtr) noexcept;
		~FDecoratorPtr();

		FDecoratorPtr& operator=(const FDecoratorPtr& DecoratorPtr);
		FDecoratorPtr& operator=(FDecoratorPtr&& DecoratorPtr) noexcept;

		// Returns a pointer to the node instance
		FNodeInstance* GetNodeInstance() const noexcept { return reinterpret_cast<FNodeInstance*>(PackedPointerAndFlags & ~FLAGS_MASK); }

		// Returns the decorator index this pointer handle references
		constexpr uint32 GetDecoratorIndex() const noexcept { return DecoratorIndex; }

		// Returns true when the pointer is valid, false otherwise
		constexpr bool IsValid() const noexcept { return PackedPointerAndFlags != 0; }

		// Returns true if this pointer handle is weak, false otherwise
		constexpr bool IsWeak() const noexcept { return (PackedPointerAndFlags & IS_WEAK_BIT) != 0; }

		// Clears the handle and renders it invalid
		void Reset();

		// Equality and inequality tests
		bool operator==(const FDecoratorPtr& RHS) const { return GetNodeInstance() == RHS.GetNodeInstance() && DecoratorIndex == RHS.DecoratorIndex; }
		bool operator!=(const FDecoratorPtr& RHS) const { return GetNodeInstance() != RHS.GetNodeInstance() || DecoratorIndex != RHS.DecoratorIndex; }
		bool operator==(const FWeakDecoratorPtr& RHS) const;
		bool operator!=(const FWeakDecoratorPtr& RHS) const;

	private:
		// Various flags stored in the pointer alignment bits, assumes an alignment of at least 4 bytes
		enum EFlags : uintptr_t
		{
			// When true, this handle is weak and it will not update the instance reference count
			IS_WEAK_BIT = 0x01,

			// Bit mask to clear all flags from the packed pointer value
			FLAGS_MASK = 0x03,
		};

		// Constructs a pointer handle to the provided instance
		FDecoratorPtr(FNodeInstance* InNodeInstance, EFlags InFlags, uint32 InDecoratorIndex);

		// Packed pointer value contains a pointer along with flags in the alignment bits
		// The pointer part points to a node instance
		uintptr_t	PackedPointerAndFlags = 0;
		uint32		DecoratorIndex = 0;				// Only need 8 bits, but use 32 since we have ample padding, avoids truncation

		friend struct FWeakDecoratorPtr;
		friend struct FExecutionContext;
	};

	/**
	 * Weak Decorator Pointer
	 * Same as a FDecoratorPtr but is strongly typed to be weak.
	 *
	 * Weak decorator pointers should be used carefully!
	 * 
	 * @see FDecoratorPtr
	 */
	struct ANIMNEXT_API FWeakDecoratorPtr
	{
		// Constructs an invalid weak pointer handle
		constexpr FWeakDecoratorPtr() noexcept = default;

		// Constructs a weak pointer handle from a shared pointer handle
		FWeakDecoratorPtr(const FDecoratorPtr& DecoratorPtr) noexcept
			: NodeInstance(DecoratorPtr.GetNodeInstance())
			, DecoratorIndex(DecoratorPtr.DecoratorIndex)
		{}

		// Constructs a weak pointer handle to the provided instance
		FWeakDecoratorPtr(FNodeInstance* InNodeInstance, uint32 InDecoratorIndex);

		// Returns a pointer to the node instance
		constexpr FNodeInstance* GetNodeInstance() const noexcept { return NodeInstance; }

		// Returns the decorator index this pointer handle references
		constexpr uint32 GetDecoratorIndex() const noexcept { return DecoratorIndex; }

		// Returns true when the pointer is valid, false otherwise
		constexpr bool IsValid() const noexcept { return NodeInstance != nullptr; }

		// Clears the handle and renders it invalid
		void Reset();

		// Equality and inequality tests
		bool operator==(const FWeakDecoratorPtr& RHS) const { return NodeInstance == RHS.NodeInstance && DecoratorIndex == RHS.DecoratorIndex; }
		bool operator!=(const FWeakDecoratorPtr& RHS) const { return NodeInstance != RHS.NodeInstance || DecoratorIndex != RHS.DecoratorIndex; }
		bool operator==(const FDecoratorPtr& RHS) const { return NodeInstance == RHS.GetNodeInstance() && DecoratorIndex == RHS.GetDecoratorIndex(); }
		bool operator!=(const FDecoratorPtr& RHS) const { return NodeInstance != RHS.GetNodeInstance() || DecoratorIndex != RHS.GetDecoratorIndex(); }

	private:
		FNodeInstance*	NodeInstance = nullptr;
		uint32			DecoratorIndex = 0;			// Only need 8 bits, but use 32 since we have ample padding, avoids truncation
	};

	//////////////////////////////////////////////////////////////////////////

	inline bool FDecoratorPtr::operator==(const FWeakDecoratorPtr& RHS) const { return GetNodeInstance() == RHS.GetNodeInstance() && DecoratorIndex == RHS.GetDecoratorIndex(); }
	inline bool FDecoratorPtr::operator!=(const FWeakDecoratorPtr& RHS) const { return GetNodeInstance() != RHS.GetNodeInstance() || DecoratorIndex != RHS.GetDecoratorIndex(); }
}
