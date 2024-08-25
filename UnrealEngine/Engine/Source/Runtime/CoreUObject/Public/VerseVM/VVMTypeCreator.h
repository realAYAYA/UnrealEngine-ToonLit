// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMContext.h"
#include "VVMLazyInitialized.h"
#include "VVMUniqueCreator.h" // IWYU pragma: keep

namespace Verse
{
struct VType;

// Get/creates hash constructed emergent types.
class VTypeCreator
{
private:
	COREUOBJECT_API static TLazyInitialized<VUniqueCreator<VType>> UniqueCreator;
	VTypeCreator() = delete;

public:
	template <typename SubType, typename... ArgumentTypes>
	static VType* GetOrCreate(FAllocationContext Context, ArgumentTypes... Arguments)
	{
		return UniqueCreator->GetOrCreate<SubType>(Context, Arguments...);
	}
};
} // namespace Verse
#endif // WITH_VERSE_VM
