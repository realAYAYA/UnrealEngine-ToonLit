// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/UnrealMemory.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "Serialization/StructuredArchive.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/Object.h"
#include "UObject/UObjectGlobals.h"

class FReferenceCollector;

namespace UE
{
	// Provides methods to interact with values of a specific type.
	struct FDynamicallyTypedValueType
	{
		enum class EContainsReferences : bool
		{
			DoesNot,
			Maybe,
		};

		constexpr FDynamicallyTypedValueType(SIZE_T InNumBytes, uint8 InMinAlignmentLogTwo, EContainsReferences InContainsReferences)
			: NumBytes(InNumBytes)
			, MinAlignmentLogTwo(InMinAlignmentLogTwo)
			, ContainsReferences(InContainsReferences)
		{
		}

		// Marks the type itself as reachable.
		virtual void MarkReachable(FReferenceCollector& Collector) = 0;

		// Marks a value of the type as reachable.
		virtual void MarkValueReachable(void* Data, FReferenceCollector& Collector) const = 0;

		virtual void InitializeValue(void* Data) const = 0;
		virtual void InitializeValueFromCopy(void* DestData, const void* SourceData) const = 0;
		virtual void DestroyValue(void* Data) const = 0;

		virtual void SerializeValue(FStructuredArchive::FSlot Slot, void* Data, const void* DefaultData) const = 0;

		virtual uint32 GetValueHash(const void* Data) const = 0;
		virtual bool AreIdentical(const void* DataA, const void* DataB) const = 0;

		SIZE_T GetNumBytes() const { return NumBytes; }
		uint8 GetMinAlignmentLogTwo() const { return MinAlignmentLogTwo; }
		uint32 GetMinAlignment() const { return 1 << MinAlignmentLogTwo; }
		EContainsReferences GetContainsReferences() const { return ContainsReferences; }

	private:
		const SIZE_T NumBytes;
		const uint8 MinAlignmentLogTwo;
		const EContainsReferences ContainsReferences;
	};

	// An value stored in some uninterpreted memory and a pointer to a type that contains methods to interpret it.
	struct FDynamicallyTypedValue
	{
		static COREUOBJECT_API FDynamicallyTypedValueType& NullType();

		FDynamicallyTypedValue() { InitializeToNull(); }
		FDynamicallyTypedValue(const FDynamicallyTypedValue& Copyee) { InitializeFromCopy(Copyee); }
		FDynamicallyTypedValue(FDynamicallyTypedValue&& Movee) { InitializeFromMove(MoveTemp(Movee)); }

		~FDynamicallyTypedValue() { Deinit(); }

		FDynamicallyTypedValue& operator=(const FDynamicallyTypedValue& Copyee)
		{
			if (this != &Copyee)
			{
				Deinit();
				InitializeFromCopy(Copyee);
			}
			return *this;
		}
		FDynamicallyTypedValue& operator=(FDynamicallyTypedValue&& Movee)
		{
			if (this != &Movee)
			{
				Deinit();
				InitializeFromMove(MoveTemp(Movee));
			}
			return *this;
		}

		// Returns a pointer to the value's data.
		const void* GetDataPointer() const { return IsInline() ? &InlineData : HeapData; }
		void* GetDataPointer() { return IsInline() ? &InlineData : HeapData; }

		// Returns the value's type.
		FDynamicallyTypedValueType& GetType() const { return *Type; }

		// Sets the value to the null state.
		void SetToNull()
		{
			Deinit();
			InitializeToNull();
		}

		// Sets the value to the initial value of a type.
		void InitializeAsType(FDynamicallyTypedValueType& NewType)
		{
			check(&NewType != nullptr);
			Deinit();
			Type = &NewType;
			AllocateData();
			Type->InitializeValue(GetDataPointer());
			MarkTypeReachableIfIncrementalReachabilityPending();			
		}

		// Returns hash of the underlying FDynamicallyTypedValue's value. Added to allow for FDynamicallyTypedValue to be used as TMap keys.
		friend uint32 GetTypeHash(const FDynamicallyTypedValue& DynamicallyTypedValue)
		{
			return DynamicallyTypedValue.GetType().GetValueHash(DynamicallyTypedValue.GetDataPointer());
		}

	private:
		void MarkTypeReachableIfIncrementalReachabilityPending()
		{
			struct FTypeReferenceCollector final : public FReferenceCollector
			{
				bool IsIgnoringArchetypeRef() const override { return false; }
				bool IsIgnoringTransient() const override { return false; }
				void HandleObjectReference(UObject*& InObject, const UObject* InReferencingObject, const FProperty* InReferencingProperty) override
				{
					if (InObject)
					{
						UE::GC::MarkAsReachable(InObject);
					}
				}
			};
			
			if (UE::GC::GIsIncrementalReachabilityPending && Type)
			{
				// nb: this is done to simulate a write barrier for this type, which
				//     enables it to behave properly with incremental gc.
				FTypeReferenceCollector Collector;
				Type->MarkReachable(Collector);
			}
		}
		
		FDynamicallyTypedValueType* Type;

		// Store pointer-sized or smaller values inline, heap allocate all others.
		union
		{
			UPTRINT InlineData;
			void* HeapData;
		};

		// Initialize this value from the primordial state to the null state.
		void InitializeToNull()
		{
			Type = &NullType();
			HeapData = nullptr;
			MarkTypeReachableIfIncrementalReachabilityPending();			
		}
		// Deinitializes this value back to the primordial state.
		void Deinit()
		{
			Type->DestroyValue(GetDataPointer());
			FreeData();
			Type = nullptr;
		}
		// Copies the data from another value to this one, which is assumed to be in the primordial state.
		void InitializeFromCopy(const FDynamicallyTypedValue& Copyee)
		{
			Type = Copyee.Type;
			AllocateData();
			Type->InitializeValueFromCopy(GetDataPointer(), Copyee.GetDataPointer());
			MarkTypeReachableIfIncrementalReachabilityPending();			
		}
		// Moves the data from another value to this one, which is assumed to be in the primordial state.
		// The source value is set to the null state.
		void InitializeFromMove(FDynamicallyTypedValue&& Movee)
		{
			// Simply copy the type and data from the source value.
			// This assumes that the data is trivially relocatable.
			Type = Movee.Type;
			InlineData = Movee.InlineData;
			MarkTypeReachableIfIncrementalReachabilityPending();			

			// Reset the source value to null.
			Movee.InitializeToNull();
		}

		// Whether the value's data is stored in InlineData or in the memory pointed to by HeapData.
		bool IsInline() const
		{
			return Type->GetNumBytes() <= sizeof(UPTRINT)
				&& Type->GetMinAlignmentLogTwo() <= UE_FORCE_CONSTEVAL(FMath::ConstExprCeilLogTwo(alignof(UPTRINT)));
		}

		// Allocates heap memory for the value if it uses it.
		void AllocateData()
		{
			if (IsInline())
			{
				// Ensure that the data is zeroed in the inline case to avoid spurious static analysis
				// errors about passing a reference to uninitialized data to InitializeValueFromCopy.
				InlineData = 0;
			}
			else
			{
				HeapData = FMemory::Malloc(Type->GetNumBytes(), Type->GetMinAlignment());
			}
		}

		// Frees heap memory for the value if it uses it.
		void FreeData()
		{
			if (!IsInline())
			{
				FMemory::Free(HeapData);
				HeapData = nullptr;
			}
		}
	};
}
