// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "Templates/SharedPointer.h"
#include "VerseVM/VVMCppClassInfo.h"
#include "VerseVM/VVMProcedure.h"
#include "VerseVM/VVMShape.h"
#include "VerseVM/VVMType.h"

class UObject;
class UVerseVMClass;

namespace Verse
{
struct VObject;
struct VProcedure;
struct VPackage;
struct VUniqueString;

/// This provides a custom comparison that allows us to do pointer-based compares of each unique string set, rather than hash-based comparisons.
struct FEmergentTypesCacheKeyFuncs : TDefaultMapKeyFuncs<TWriteBarrier<VUniqueStringSet>, TWriteBarrier<VEmergentType>, /*bInAllowDuplicateKeys*/ false>
{
public:
	static bool Matches(KeyInitType A, KeyInitType B);
	static bool Matches(KeyInitType A, const VUniqueStringSet& B);
	static uint32 GetKeyHash(KeyInitType Key);
	static uint32 GetKeyHash(const VUniqueStringSet& Key);
};

/// A sequence of fields and blocks in a class body.
/// May represent either a single class, or the flattened combination of a subclass and its superclasses.
struct VConstructor : VCell
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VCell);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	struct VEntry
	{
		/// When non-null, the name of this field. When null, this entry represents a block.
		TWriteBarrier<VUniqueString> Name;

		/// When bDynamic, a VProcedure for a default initializer or block, or nothing for an uninitialized field.
		/// Otherwise, a constant VValue for a default field value (which may be a VProcedure for functions, which bind Self lazily).
		TWriteBarrier<VValue> Value;
		bool bDynamic;

		static VEntry Constant(FAllocationContext Context, VUniqueString& InField, VValue InValue)
		{
			return VEntry{
				{Context, InField},
				{Context, InValue},
				false
            };
		}

		static VEntry Field(FAllocationContext Context, VUniqueString& InField)
		{
			return {
				{Context, InField},
				{},
				true
            };
		}

		static VEntry FieldInitializer(FAllocationContext Context, VUniqueString& InField, VProcedure& Code)
		{
			return {
				{Context,      InField},
				{Context, VValue(Code)},
				true
            };
		}

		static VEntry Block(FAllocationContext Context, VProcedure& Code)
		{
			return {
				{},
				{Context, VValue(Code)},
				true
            };
		}

		VProcedure* Initializer() const
		{
			if (bDynamic && Value.Get())
			{
				return &Value.Get().StaticCast<VProcedure>();
			}
			else
			{
				return nullptr;
			}
		}
	};

	uint32 NumEntries;
	VEntry Entries[];

	static VConstructor& New(FAllocationContext Context, const TArray<VEntry>& InEntries)
	{
		size_t NumBytes = offsetof(VConstructor, Entries) + InEntries.Num() * sizeof(Entries[0]);
		return *new (Context.AllocateFastCell(NumBytes)) VConstructor(Context, InEntries);
	}

	COREUOBJECT_API void ToStringImpl(FStringBuilderBase& Builder, FAllocationContext Context, const FCellFormatter& Formatter);

private:
	VConstructor(FAllocationContext Context, const TArray<VEntry>& InEntries)
		: VCell(Context, &GlobalTrivialEmergentType.Get(Context))
		, NumEntries(InEntries.Num())
	{
		for (uint32 Index = 0; Index < NumEntries; ++Index)
		{
			new (&Entries[Index]) VEntry(InEntries[Index]);
		}
	}
};

struct VClass : VType
{
	DECLARE_DERIVED_VCPPCLASSINFO(COREUOBJECT_API, VType);
	COREUOBJECT_API static TGlobalTrivialEmergentTypePtr<&StaticCppClassInfo> GlobalTrivialEmergentType;

	enum class EKind : uint8
	{
		Class,
		Struct,
		Interface
	};

	/// Vends an emergent type based on requested fields to override in the class archetype instantiation.
	VEmergentType& GetOrCreateEmergentTypeForArchetype(FAllocationContext Context, VUniqueStringSet& ArchetypeFieldNames);

	VUTF8String& GetName() const { return *ClassName; }
	EKind GetKind() const { return Kind; }
	VConstructor& GetConstructor() { return *Constructor; }
	UVerseVMClass* GetOrCreateUClass(FAllocationContext Context) { return AssociatedUClass ? reinterpret_cast<UVerseVMClass*>(AssociatedUClass.Get().AsUObject()) : CreateUClass(Context); }

	/// Allocate a new VObject. Also returns a sequence of VProcedures to invoke to finish the object's construction.
	/// `ArchetypeValues` should match the order of IDs in `ArchetypeFields`.
	VObject& NewVObject(FAllocationContext Context, VUniqueStringSet& ArchetypeFields, const TArray<VValue>& ArchetypeValues, TArray<VProcedure*>& OutInitializers);

	/// Allocate a new UObject. Also returns a sequence of VProcedures to invoke to finish the object's construction.
	/// `ArchetypeValues` should match the order of IDs in `ArchetypeFields`.
	UObject* NewUObject(FAllocationContext Context, VUniqueStringSet& ArchetypeFields, const TArray<VValue>& ArchetypeValues, TArray<VProcedure*>& OutInitializers);

	/**
	 * Creates a new class.
	 *
	 * @param Name        Name or null.
	 * @param Kind        Class, Struct or Interface.
	 * @param Constructor The sequence of fields and blocks in the class body.
	 * @param Inherited   An array of base classes in order of inheritance.
	 * @param Scope       Containing package or null.
	 */
	static VClass& New(FAllocationContext Context, VUTF8String* Name, EKind Kind, VConstructor& Constructor, const TArray<VClass*>& Inherited, VPackage* Scope);

protected:
	VClass(FAllocationContext Context, VUTF8String* Name, EKind Kind, VConstructor& InConstructor, const TArray<VClass*>& InInherited, VPackage* InScope);

	/// Append to `Entries` those elements of `Base` which are not already overridden, indicated by `Fields`.
	COREUOBJECT_API static void Extend(TSet<VUniqueString*>& Fields, TArray<VConstructor::VEntry>& Entries, const VConstructor& Base);

	// Helper to find initializer procedures after archetype fields have been set on an object
	void GatherInitializers(VUniqueStringSet& ArchetypeFields, TArray<VProcedure*>& OutInitializers);

	/// Creates an associated UClass for this VClass
	UVerseVMClass* CreateUClass(FAllocationContext Context);

	TWriteBarrier<VUTF8String> ClassName;

	/// The package this class is in
	TWriteBarrier<VPackage> Scope;

	// TODO: (yiliang.siew) This should be a weak map when we can support it in the GC. https://jira.it.epicgames.com/browse/SOL-5312
	/// This is a cache that allows for fast vending of emergent types based on the fields being overridden.
	TMap<TWriteBarrier<VUniqueStringSet>, TWriteBarrier<VEmergentType>, FDefaultSetAllocator, FEmergentTypesCacheKeyFuncs> EmergentTypesCache;

	/// The combined sequence of initializers and blocks in this class and its superclasses, in execution order.
	/// Actual object construction may further override some elements of this sequence.
	TWriteBarrier<VConstructor> Constructor;

	/// An associated UClass allows this VClass to create UObject instances
	TWriteBarrier<VValue> AssociatedUClass;

	EKind Kind; // Stored here to share alignment space with NumInherited

	uint32 NumInherited;
	TWriteBarrier<VClass> Inherited[];
};
};     // namespace Verse
#endif // WITH_VERSE_VM
