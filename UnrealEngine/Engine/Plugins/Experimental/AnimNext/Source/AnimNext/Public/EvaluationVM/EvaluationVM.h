// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "Animation/AnimCurveFilter.h"
#include "EvaluationVM/EvaluationFlags.h"
#include "EvaluationVM/KeyframeState.h"
#include "ReferencePose.h"
#include "Misc/Crc.h"

// If you wish to use a custom type with a stack in the FEvaluationVM, you'll have to enable
// its usage through this macro to ensure that UE::AnimNext::GetTypeID() is declared.
// Use this macro inside the UE::AnimNext namespace
#define ANIM_NEXT_ENABLE_EVALUATION_STACK_USAGE(Type) \
	template<> \
	constexpr uint32 GetTypeID<Type>() \
	{ \
		return UE::AnimNext::ConstexprStringFnv32(#Type); \
	}

namespace UE::AnimNext
{
	constexpr uint32 ConstexprStringFnv32(const char* StringLiteral)
	{
		constexpr uint32 Offset = 0x811c9dc5;
		constexpr uint32 Prime = 0x01000193;

		const char* CharPtr = StringLiteral;

		uint32 Fnv = Offset;
		while (*CharPtr != 0)
		{
			Fnv ^= *CharPtr++;
			Fnv *= Prime;
		}

		return Fnv;
	}

	// Helper function that returns a UID for the specified type
	// @see ANIM_NEXT_ENABLE_EVALUATION_STACK_USAGE
	template<typename Type>
	constexpr uint32 GetTypeID()
	{
		checkf(false, TEXT("Not implemented! See ANIM_NEXT_ENABLE_EVALUATION_STACK_USAGE for details and register your type"));
		return 0;
	}

	// Helper function that destructs a pointer of the provided type
	template<typename Type>
	inline void DestructType(void* Ptr)
	{
		reinterpret_cast<Type*>(Ptr)->~Type();
	}

	template<typename Type>
	inline TFunction<void(void*)> GetTypeDestructor()
	{
		if constexpr (std::is_trivially_destructible_v<Type>)
		{
			return TFunction<void(void*)>();
		}
		else
		{
			return TFunction<void(void*)>(&DestructType<Type>);
		}
	}

	/*
	 * Evaluation VM Stack Name
	 * 
	 * A small struct to wrap an FName and cache its type hash.
	 */
	struct FEvaluationVMStackName final
	{
		// Implicitly coerce or construct a stack name from an FName
		FEvaluationVMStackName(const FName& InName)
			: Name(InName)
			, NameHash(GetTypeHash(InName))
		{}

		FName Name;
		uint32 NameHash = 0;
	};

	/*
	 * Evaluation VM Stack Entry
	 * 
	 * A small header for each stack entry.
	 */
	struct alignas(16) FEvaluationVMStackEntry final
	{
		union
		{
			// Pointer to the previous entry beneath us on the stack
			FEvaluationVMStackEntry* Prev = nullptr;

			// Dummy padding to ensure this structure remains 16 bytes even on 32-bit architectures
			uint64 DummyPadding;
		};

		// Allocated size of this entry, includes sizeof(FEvaluationVMStackEntry)
		uint32 Size = 0;

		// Metadata flags
		uint32 Flags = 0;

		// Entry's actual data follows in memory, aligned to 16 bytes

		void* GetValuePtr() { return reinterpret_cast<uint8*>(this) + sizeof(FEvaluationVMStackEntry); }
		const void* GetValuePtr() const { return reinterpret_cast<const uint8*>(this) + sizeof(FEvaluationVMStackEntry); }
	};

	static_assert(sizeof(FEvaluationVMStackEntry) == 16, "Expected 16 bytes to ensure proper alignment");

	/*
	 * Evaluation VM Stack
	 * 
	 * Represents a named and typed VM stack.
	 */
	struct FEvaluationVMStack final
	{
		// Name of the stack
		FName Name;

		// Type ID used for runtime checks to make sure we always read/write using the same type
		// @see ANIM_NEXT_ENABLE_EVALUATION_STACK_USAGE
		uint32 TypeID = 0;

		// Pointer to the entry on top of the stack or nullptr if the stack is empty
		FEvaluationVMStackEntry* Top = nullptr;

		// For non-trivially destructible types, the destructor to call
		// Only used if the stack is not empty when it is destroyed
		TFunction<void(void*)> TypeDestructor;

		FEvaluationVMStack() = default;

		// Disable copy but enable move
		FEvaluationVMStack(const FEvaluationVMStack&) = delete;
		FEvaluationVMStack(FEvaluationVMStack&&) = default;
		FEvaluationVMStack& operator=(const FEvaluationVMStack&) = delete;
		FEvaluationVMStack& operator=(FEvaluationVMStack&&) = default;

		// The destructor will free remaining entries
		~FEvaluationVMStack();
	};

	/*
	 * Evaluation VM
	 *
	 * This struct holds the internal state when we execute and FEvaluationProgram.
	 * Our virtual machine is stack based, values are generally pushed and popped.
	 * 
	 * @see FEvaluationProgram
	 */
	struct ANIMNEXT_API FEvaluationVM
	{
		FEvaluationVM() = default;
		FEvaluationVM(EEvaluationFlags InEvaluationFlags, const UE::AnimNext::FReferencePose& InReferencePose, int32 InCurrentLOD);

		// Allow move
		FEvaluationVM(FEvaluationVM&&) = default;
		FEvaluationVM& operator=(FEvaluationVM&&) = default;

		// Returns true if we are fully initialized
		[[nodiscard]] bool IsValid() const;

		// Returns the evaluation flags
		[[nodiscard]] EEvaluationFlags GetFlags() const { return EvaluationFlags; }

		// Pushes and moves the specified value on its evaluation stack
		template<typename ValueType>
		void PushValue(const FEvaluationVMStackName& StackName, ValueType&& Value);

		// Pops the top value from the evaluation stack
		// Returns true if the stack was not empty and a value is returned, false otherwise
		template<typename ValueType>
		[[nodiscard]] bool PopValue(const FEvaluationVMStackName& StackName, ValueType& OutValue);

		// Returns an immutable pointer to a value at some offset from the top of the evaluation stack
		// An offset of 0 means the top of the stack
		// Returns valid pointer if an entry exists, nullptr otherwise
		template<typename ValueType>
		[[nodiscard]] const ValueType* PeekValue(const FEvaluationVMStackName& StackName, uint32 Offset) const;

		// Returns a mutable pointer to a value at some offset from the top of the evaluation stack
		// An offset of 0 means the top of the stack
		// Returns valid pointer if an entry exists, nullptr otherwise
		template<typename ValueType>
		[[nodiscard]] ValueType* PeekValueMutable(const FEvaluationVMStackName& StackName, uint32 Offset);

		// Trims internal containers to only use as much memory as required to avoid slack
		void Shrink();

		// Returns a reference keyframe (bind pose or additive identity)
		[[nodiscard]] FKeyframeState MakeReferenceKeyframe(bool bAdditiveKeyframe) const;

		// Returns an uninitialized keyframe with memory pre-allocated
		[[nodiscard]] FKeyframeState MakeUninitializedKeyframe(bool bAdditiveKeyframe) const;

	private:
		// Disallow copy
		FEvaluationVM(const FEvaluationVM&) = delete;
		FEvaluationVM& operator=(const FEvaluationVM&) = delete;

		// Retrieves and optionally creates the stack (if not found) by the specified name
		FEvaluationVMStack& GetOrCreateStack(const FEvaluationVMStackName& StackName, uint32 TypeID);

		// Returns the specified stack if found, nullptr otherwise
		FEvaluationVMStack* FindStack(const FEvaluationVMStackName& StackName, uint32 TypeID);
		const FEvaluationVMStack* FindStack(const FEvaluationVMStackName& StackName, uint32 TypeID) const;

		const void* PeekValueImpl(const FEvaluationVMStackName& StackName, uint32 TypeID, uint32 Offset) const;

		// Various internal stacks that tasks can use (e.g. keyframe state)
		TMap<FName, FEvaluationVMStack> InternalStacks;

		// Reference pose
		const UE::AnimNext::FReferencePose* ReferencePose = nullptr;

		// Current LOD we are evaluating at
		int32 CurrentLOD = 0;

		// Flags that control what we wish to evaluate
		EEvaluationFlags EvaluationFlags = EEvaluationFlags::All;

		// Default curve filter
		UE::Anim::FCurveFilter CurveFilter;

		// TODO: Use a stack based allocator for the stack entries and mark freed entries on Pop and coalesce when we can
		// Can allocate stack segments from memstack, limit allocation size to 1024 bytes, allocate segments of 8192 bytes
	};

	//////////////////////////////////////////////////////////////////////////
	// Inline implementations

	template<typename ValueType>
	inline void FEvaluationVM::PushValue(const FEvaluationVMStackName& StackName, ValueType&& Value)
	{
		static_assert(alignof(ValueType) <= alignof(FEvaluationVMStackEntry), "Expected an alignment smaller or equal to 16");

		const uint32 TypeID = GetTypeID<ValueType>();
		FEvaluationVMStack& Stack = GetOrCreateStack(StackName, TypeID);

		if (!ensureMsgf(Stack.TypeID == TypeID, TEXT("Type mismatch! This evaluation stack is being queried with a different type than it was created with")))
		{
			return;
		}

		const uint32 EntrySize = sizeof(FEvaluationVMStackEntry) + sizeof(Value);
		FEvaluationVMStackEntry* EntryPtr = reinterpret_cast<FEvaluationVMStackEntry*>(FMemory::Malloc(EntrySize, alignof(FEvaluationVMStackEntry)));
		EntryPtr->Size = EntrySize;
		EntryPtr->Prev = Stack.Top;
		EntryPtr->Flags = 0;

		ValueType* ValuePtr = reinterpret_cast<ValueType*>(EntryPtr->GetValuePtr());
		new(ValuePtr) ValueType(MoveTemp(Value));

		if (Stack.Top == nullptr)
		{
			Stack.TypeDestructor = GetTypeDestructor<ValueType>();
		}

		Stack.Top = EntryPtr;
	}

	template<typename ValueType>
	[[nodiscard]] inline bool FEvaluationVM::PopValue(const FEvaluationVMStackName& StackName, ValueType& OutValue)
	{
		FEvaluationVMStack* Stack = FindStack(StackName, GetTypeID<ValueType>());
		if (Stack == nullptr)
		{
			// Stack not found
			return false;
		}

		FEvaluationVMStackEntry* EntryPtr = Stack->Top;
		if (EntryPtr == nullptr)
		{
			// Stack is empty
			return false;
		}

		Stack->Top = EntryPtr->Prev;

		ValueType* ValuePtr = reinterpret_cast<ValueType*>(EntryPtr->GetValuePtr());
		OutValue = MoveTemp(*ValuePtr);

		ValuePtr->~ValueType();
		FMemory::Free(EntryPtr);

		return true;
	}

	template<typename ValueType>
	[[nodiscard]] inline const ValueType* FEvaluationVM::PeekValue(const FEvaluationVMStackName& StackName, uint32 Offset) const
	{
		return reinterpret_cast<const ValueType*>(PeekValueImpl(StackName, GetTypeID<ValueType>(), Offset));
	}

	template<typename ValueType>
	[[nodiscard]] inline ValueType* FEvaluationVM::PeekValueMutable(const FEvaluationVMStackName& StackName, uint32 Offset)
	{
		// Re-use const impl and cast it away
		return const_cast<ValueType*>(reinterpret_cast<const ValueType*>(PeekValueImpl(StackName, GetTypeID<ValueType>(), Offset)));
	}

	//////////////////////////////////////////////////////////////////////////
	// Various commonly used VM stacks

	// A stack of FKeyframeState instances used when sampling sequences and blending their results
	extern const FEvaluationVMStackName KEYFRAME_STACK_NAME;
	ANIM_NEXT_ENABLE_EVALUATION_STACK_USAGE(TUniquePtr<FKeyframeState>)
}
