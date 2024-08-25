// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "HAL/UnrealMemory.h"
#include "Templates/IsTriviallyCopyConstructible.h"
#include "Templates/UnrealTypeTraits.h"
#include "Traits/UseBitwiseSwap.h"
#include <new> // IWYU pragma: export

#include "Templates/IsTriviallyCopyAssignable.h"
#include "Templates/IsTriviallyDestructible.h"


namespace UE::Core::Private::MemoryOps
{
	template <typename DestinationElementType, typename SourceElementType>
	struct TCanBitwiseRelocate
	{
		enum
		{
			Value =
				std::is_same_v<DestinationElementType, SourceElementType> ||
				TAnd<
					TIsBitwiseConstructible<DestinationElementType, SourceElementType>,
					TIsTriviallyDestructible<SourceElementType>
				>::Value
		};
	};
}

/**
 * Default constructs a range of items in memory.
 *
 * @param	Elements	The address of the first memory location to construct at.
 * @param	Count		The number of elements to destruct.
 */
template <typename ElementType, typename SizeType>
FORCEINLINE void DefaultConstructItems(void* Address, SizeType Count)
{
	if constexpr (TIsZeroConstructType<ElementType>::Value)
	{
		FMemory::Memset(Address, 0, sizeof(ElementType) * Count);
	}
	else
	{
		ElementType* Element = (ElementType*)Address;
		while (Count)
		{
			new (Element) ElementType;
			++Element;
			--Count;
		}
	}
}

/**
 * Destructs a single item in memory.
 *
 * @param	Elements	A pointer to the item to destruct.
 *
 * @note: This function is optimized for values of T, and so will not dynamically dispatch destructor calls if T's destructor is virtual.
 */
template <typename ElementType>
FORCEINLINE void DestructItem(ElementType* Element)
{
	if constexpr (!TIsTriviallyDestructible<ElementType>::Value)
	{
		// We need a typedef here because VC won't compile the destructor call below if ElementType itself has a member called ElementType
		typedef ElementType DestructItemsElementTypeTypedef;

		Element->DestructItemsElementTypeTypedef::~DestructItemsElementTypeTypedef();
	}
}

/**
 * Destructs a range of items in memory.
 *
 * @param	Elements	A pointer to the first item to destruct.
 * @param	Count		The number of elements to destruct.
 *
 * @note: This function is optimized for values of T, and so will not dynamically dispatch destructor calls if T's destructor is virtual.
 */
template <typename ElementType, typename SizeType>
FORCEINLINE void DestructItems(ElementType* Element, SizeType Count)
{
	if constexpr (!TIsTriviallyDestructible<ElementType>::Value)
	{
		while (Count)
		{
			// We need a typedef here because VC won't compile the destructor call below if ElementType itself has a member called ElementType
			typedef ElementType DestructItemsElementTypeTypedef;

			Element->DestructItemsElementTypeTypedef::~DestructItemsElementTypeTypedef();
			++Element;
			--Count;
		}
	}
}

/**
 * Constructs a range of items into memory from a set of arguments.  The arguments come from an another array.
 *
 * @param	Dest		The memory location to start copying into.
 * @param	Source		A pointer to the first argument to pass to the constructor.
 * @param	Count		The number of elements to copy.
 */
template <typename DestinationElementType, typename SourceElementType, typename SizeType>
FORCEINLINE void ConstructItems(void* Dest, const SourceElementType* Source, SizeType Count)
{
	if constexpr (TIsBitwiseConstructible<DestinationElementType, SourceElementType>::Value)
	{
		if (Count)
		{
			FMemory::Memcpy(Dest, Source, sizeof(SourceElementType) * Count);
		}
	}
	else
	{
		while (Count)
		{
			new (Dest) DestinationElementType(*Source);
			++(DestinationElementType*&)Dest;
			++Source;
			--Count;
		}
	}
}

/**
 * Copy assigns a range of items.
 *
 * @param	Dest		The memory location to start assigning to.
 * @param	Source		A pointer to the first item to assign.
 * @param	Count		The number of elements to assign.
 */
template <typename ElementType, typename SizeType>
FORCEINLINE void CopyAssignItems(ElementType* Dest, const ElementType* Source, SizeType Count)
{
	if constexpr (TIsTriviallyCopyAssignable<ElementType>::Value)
	{
		FMemory::Memcpy(Dest, Source, sizeof(ElementType) * Count);
	}
	else
	{
		while (Count)
		{
			*Dest = *Source;
			++Dest;
			++Source;
			--Count;
		}
	}
}

/**
 * Relocates a single items to a new memory location as a new type. This is a so-called 'destructive move' for which
 * there is no single operation in C++ but which can be implemented very efficiently in general.
 *
 * @param	Dest		The memory location to relocate to.
 * @param	Source		A pointer to the first item to relocate.
 */
template <typename DestinationElementType, typename SourceElementType, typename SizeType>
FORCEINLINE void RelocateConstructItem(void* Dest, const SourceElementType* Source)
{
	// Do a bitwise relocate if TCanBitwiseRelocate says we can, but not if the type involved is known not to be bitwise-swappable
	if constexpr (UE::Core::Private::MemoryOps::TCanBitwiseRelocate<DestinationElementType, SourceElementType>::Value && (!std::is_same_v<DestinationElementType, SourceElementType> || TUseBitwiseSwap<SourceElementType>::Value))
	{
		/* All existing UE containers seem to assume trivial relocatability (i.e. memcpy'able) of their members,
		 * so we're going to assume that this is safe here.  However, it's not generally possible to assume this
		 * in general as objects which contain pointers/references to themselves are not safe to be trivially
		 * relocated.
		 *
		 * However, it is not yet possible to automatically infer this at compile time, so we can't enable
		 * different (i.e. safer) implementations anyway. */

		FMemory::Memmove(Dest, Source, sizeof(SourceElementType));
	}
	else
	{
		// We need a typedef here because VC won't compile the destructor call below if SourceElementType itself has a member called SourceElementType
		typedef SourceElementType RelocateConstructItemsElementTypeTypedef;

		new (Dest) DestinationElementType(*Source);
		Source->RelocateConstructItemsElementTypeTypedef::~RelocateConstructItemsElementTypeTypedef();
	}
}

/**
 * Relocates a range of items to a new memory location as a new type. This is a so-called 'destructive move' for which
 * there is no single operation in C++ but which can be implemented very efficiently in general.
 *
 * @param	Dest		The memory location to relocate to.
 * @param	Source		A pointer to the first item to relocate.
 * @param	Count		The number of elements to relocate.
 */
template <typename DestinationElementType, typename SourceElementType, typename SizeType>
FORCEINLINE void RelocateConstructItems(void* Dest, const SourceElementType* Source, SizeType Count)
{
	if constexpr (UE::Core::Private::MemoryOps::TCanBitwiseRelocate<DestinationElementType, SourceElementType>::Value)
	{
		/* All existing UE containers seem to assume trivial relocatability (i.e. memcpy'able) of their members,
		 * so we're going to assume that this is safe here.  However, it's not generally possible to assume this
		 * in general as objects which contain pointers/references to themselves are not safe to be trivially
		 * relocated.
		 *
		 * However, it is not yet possible to automatically infer this at compile time, so we can't enable
		 * different (i.e. safer) implementations anyway. */

		FMemory::Memmove(Dest, Source, sizeof(SourceElementType) * Count);
	}
	else
	{
		while (Count)
		{
			// We need a typedef here because VC won't compile the destructor call below if SourceElementType itself has a member called SourceElementType
			typedef SourceElementType RelocateConstructItemsElementTypeTypedef;

			new (Dest) DestinationElementType(*Source);
			++(DestinationElementType*&)Dest;
			(Source++)->RelocateConstructItemsElementTypeTypedef::~RelocateConstructItemsElementTypeTypedef();
			--Count;
		}
	}
}

/**
 * Move constructs a range of items into memory.
 *
 * @param	Dest		The memory location to start moving into.
 * @param	Source		A pointer to the first item to move from.
 * @param	Count		The number of elements to move.
 */
template <typename ElementType, typename SizeType>
FORCEINLINE void MoveConstructItems(void* Dest, const ElementType* Source, SizeType Count)
{
	if constexpr (TIsTriviallyCopyConstructible<ElementType>::Value)
	{
		FMemory::Memmove(Dest, Source, sizeof(ElementType) * Count);
	}
	else
	{
		while (Count)
		{
			new (Dest) ElementType((ElementType&&)*Source);
			++(ElementType*&)Dest;
			++Source;
			--Count;
		}
	}
}

/**
 * Move assigns a range of items.
 *
 * @param	Dest		The memory location to start move assigning to.
 * @param	Source		A pointer to the first item to move assign.
 * @param	Count		The number of elements to move assign.
 */
template <typename ElementType, typename SizeType>
FORCEINLINE void MoveAssignItems(ElementType* Dest, const ElementType* Source, SizeType Count)
{
	if constexpr (TIsTriviallyCopyAssignable<ElementType>::Value)
	{
		FMemory::Memmove(Dest, Source, sizeof(ElementType) * Count);
	}
	else
	{
		while (Count)
		{
			*Dest = (ElementType&&)*Source;
			++Dest;
			++Source;
			--Count;
		}
	}
}

template <typename ElementType, typename SizeType>
FORCEINLINE bool CompareItems(const ElementType* A, const ElementType* B, SizeType Count)
{
	if constexpr (TTypeTraits<ElementType>::IsBytewiseComparable)
	{
		return !Count || !FMemory::Memcmp(A, B, sizeof(ElementType) * Count);
	}
	else
	{
		while (Count)
		{
			if (!(*A == *B))
			{
				return false;
			}

			++A;
			++B;
			--Count;
		}

		return true;
	}
}
