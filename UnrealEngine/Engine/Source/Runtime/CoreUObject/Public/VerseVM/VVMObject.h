// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "VerseVM/VVMRestValue.h"
#include "VerseVM/VVMShape.h"

namespace Verse
{
struct VClass;
struct VProcedure;
struct VUniqueString;

static constexpr uint8 IsStructBit = 4;

/// A Verse object that may store fields and associated values for those fields on it.
/// An object points to an emergent type, which in turn points to a "shape".
/// A "shape" is a dynamic memory layout of fields and their offsets.
struct VObject : VHeapValue
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VHeapValue);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	/// Allocate a new object with the given shape, populated with placeholders
	static VObject& NewUninitialized(FAllocationContext Context, VEmergentType& InEmergentType);

	const VValue LoadField(FAllocationContext Context, VUniqueString& Name);

	/// Use this when you are retrieving a `var` from an object and not what the `var` points to.
	/// The data is retrieved from the object, rather than the shape.
	VRestValue& GetFieldSlot(FAllocationContext Context, VUniqueString& Name);

	void SetField(FAllocationContext Context, VUniqueString& Name, VValue Value);

	bool IsStruct() const { return !!(Misc2 & IsStructBit); };

private:
	COREUOBJECT_API bool EqualImpl(FRunningContext Context, VCell* Other, const TFunction<void(::Verse::VValue, ::Verse::VValue)>& HandlePlaceholder);
	COREUOBJECT_API uint32 GetTypeHashImpl();

	VObject(FAllocationContext Context, VEmergentType& InEmergentType);

	/*
	 * Mutable variables store their data as a `VRestValue`.
	 * It's not an array of `VValue`s because you can potentially load a class member before actually defining it. i.e.
	 *
	 * ```
	 * c := class {x:int}
	 * c := C{}
	 * Foo(c.X) # allocates a placeholder
	 * c.X := 1  # This is the first time `c.X` actually gets defined.
	 * ```
	 *
	 * This stores the actual data for individual fields. Some constants and procedures are stored in the shape, not the
	 * object (since then there's no need to do an unnecessary index lookup).
	 *
	 * The mapping of offsets to each field are stored in the emergent type's "shape".  The reason why the object
	 * doesn't just store the mapping of fields to data itself is that it will eventually help when we implement inline
	 * caches for retrieving fields on objects. It also helps reduce memory usage because multiple objects can share
	 * the same hash table that describes their layouts.
	 */
	VRestValue Data[];
};
} // namespace Verse
#endif // WITH_VERSE_VM
