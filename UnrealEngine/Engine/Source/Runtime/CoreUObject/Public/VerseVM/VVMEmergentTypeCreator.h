// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "HAL/Platform.h"

namespace Verse
{
struct FAllocationContext;
struct VCppClassInfo;
struct VEmergentType;
struct VType;
struct VShape;

template <typename T>
struct TGlobalHeapPtr;

template <typename T>
struct TLazyInitialized;

template <typename Type>
class VUniqueCreator;

// Get/creates hash constructed emergent types.
class VEmergentTypeCreator
{
	static TLazyInitialized<VUniqueCreator<VEmergentType>> UniqueCreator;
	static bool bIsInitialized;

	VEmergentTypeCreator() = delete;

public:
	COREUOBJECT_API static void Initialize();
	COREUOBJECT_API static VEmergentType* GetOrCreate(FAllocationContext Context, VType* Type, VCppClassInfo* CppClassInfo);
	COREUOBJECT_API static VEmergentType* GetOrCreate(FAllocationContext Context, VShape* InShape, VType* Type, VCppClassInfo* CppClassInfo);

	COREUOBJECT_API static TGlobalHeapPtr<VEmergentType> EmergentTypeForEmergentType;
	COREUOBJECT_API static TGlobalHeapPtr<VEmergentType> EmergentTypeForType;
};
}; // namespace Verse

#endif // WITH_VERSE_VM
