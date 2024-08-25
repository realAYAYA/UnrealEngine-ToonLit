// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VVMCell.h"
#include "VVMContext.h"
#include <new>

namespace Verse
{
struct VCppClassInfo;

template <typename T>
struct TGlobalHeapPtr;

// Represents Verse types, which may be independent of object shape, and independent of C++ type.
struct VType : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);

protected:
	COREUOBJECT_API explicit VType(FAllocationContext Context, VEmergentType* Type);
};

// Define the template function VType::VisitReferencesImpl here so it can be called by subclasses.
DEFINE_TRIVIAL_VISIT_REFERENCES(VType);

struct VTrivialType : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);

	COREUOBJECT_API static TGlobalHeapPtr<VTrivialType> Singleton;

	static void Initialize(FAllocationContext Context);

private:
	VTrivialType(FAllocationContext Context);
};

} // namespace Verse
#endif // WITH_VERSE_VM
