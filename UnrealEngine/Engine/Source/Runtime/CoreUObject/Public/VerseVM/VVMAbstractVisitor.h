// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_VERSE_VM || defined(__INTELLISENSE__)

#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "VVMCell.h"
#include "VVMValue.h"
#include "VVMWriteBarrier.h"

class FArchive;
class UObject;

namespace Verse
{
struct VCell;
struct VRestValue;

struct FAbstractVisitor
{
	UE_NONCOPYABLE(FAbstractVisitor);

	static constexpr bool bIsAbstractVisitor = true;

	enum class EReferrerType
	{
		Cell,
		UObject,
	};

	// The referrer token represents the cell or object that is currently being visited
	struct FReferrerToken
	{
		FReferrerToken(VCell* Cell);
		FReferrerToken(UObject* Object);

		EReferrerType GetType() const;
		bool IsCell() const;
		VCell* AsCell() const;
		bool IsUObject() const;
		UObject* AsUObject() const;

	private:
		static constexpr uint64 EncodingBits = 0b1;
		uint64 EncodedBits{0};
	};

	// A stack based context to maintain the chain of referrers
	struct FReferrerContext
	{
		FReferrerContext(FAbstractVisitor& InVisitor, FReferrerToken InReferrer);
		~FReferrerContext();

		FReferrerToken GetReferrer() const { return Referrer; }
		FReferrerContext* GetPrevious() const { return Previous; }

	private:
		FAbstractVisitor& Visitor;
		FReferrerToken Referrer;
		FReferrerContext* Previous;
	};

	virtual ~FAbstractVisitor() = default;

	// The context provides information about the current cell being visited
	FReferrerContext* GetContext() const
	{
		return Context;
	}

	// Override the following methods to constomize how different values will be processed.  For visitors that just need
	// to enumerate VCell and UObject references, these are the only methods that need to be overridden
	virtual void VisitNonNull(VCell*& InCell, const TCHAR* ElementName);
	virtual void VisitNonNull(UObject*& InObject, const TCHAR* ElementName);
	virtual void VisitAuxNonNull(void* InAux, const TCHAR* ElementName);

	// This method is only invoked by VCell to visit the emergent type of the cell.  It should not be
	// called in any other situtation.
	virtual void VisitEmergentType(const VCell* InEmergentType);

	// POD type visitors
	virtual void Visit(bool& bValue, const TCHAR* ElementName);
	virtual void Visit(FString& Value, const TCHAR* ElementName);
	virtual void Visit(uint64& Value, const TCHAR* ElementName);
	virtual void Visit(int64& Value, const TCHAR* ElementName);

	// Override the following methods to handle nesting of elements.  Begin/EndObject are intended for when
	// objects are elements in arrays.
	virtual void BeginArray(const TCHAR* ElementName, uint64& NumElements);
	virtual void EndArray();
	virtual void BeginSet(const TCHAR* ElementName, uint64& NumElements);
	virtual void EndSet();
	virtual void BeginMap(const TCHAR* ElementName, uint64& NumElements);
	virtual void EndMap();
	virtual void BeginObject(const TCHAR* ElementName = nullptr);
	virtual void EndObject();

	virtual bool IsMarked(VCell* InCell, const TCHAR* ElementName) { return true; }

	// The default implementation of the following methods just check for null values and then forward to the non-null variants
	virtual void Visit(VCell*& InCell, const TCHAR* ElementName);
	virtual void Visit(UObject*& InObject, const TCHAR* ElementName);
	virtual void VisitAux(void* InAux, const TCHAR* ElementName);

	// The default implementation looks for either a VCell or UObject pointer and invokes the proper Visit method if found
	virtual void Visit(VValue& Value, const TCHAR* ElementName);

	// The default implementation forwards the call to the VRestValue::Visit method
	virtual void Visit(VRestValue& Value, const TCHAR* ElementName);

	template <typename T>
	FORCEINLINE void Visit(TWriteBarrier<T>& Value, const TCHAR* ElementName)
	{
		using TValue = typename TWriteBarrier<T>::TValue;
		if (IsLoading())
		{
			if constexpr (TWriteBarrier<T>::bIsVValue)
			{
				TValue Scratch = TValue{};
				Visit(Scratch, ElementName);
				Value.Set(GetLoadingContext(), Scratch);
			}
			else if constexpr (TWriteBarrier<T>::bIsAux)
			{
			}
			else
			{
				VCell* Scratch = nullptr;
				Visit(Scratch, ElementName);
				if (Scratch != nullptr)
				{
					Value.Set(GetLoadingContext(), Scratch->StaticCast<T>());
				}
				else
				{
					Value.Reset();
				}
			}
		}
		else
		{
			if constexpr (TWriteBarrier<T>::bIsVValue)
			{
				TValue Scratch = Value.Get();
				Visit(Scratch, ElementName);
			}
			else if constexpr (TWriteBarrier<T>::bIsAux)
			{
			}
			else
			{
				VCell* Scratch = Value.Get();
				Visit(Scratch, ElementName);
			}
		}
	}

	// Simple arrays
	template <typename T>
	FORCEINLINE void Visit(T Begin, T End)
	{
		for (; Begin != End; ++Begin)
		{
			Visit(*Begin, TEXT(""));
		}
	}

	// Arrays
	template <typename ElementType, typename AllocatorType>
	FORCEINLINE void Visit(const TArray<ElementType, AllocatorType>& Values, const TCHAR* ElementName)
	{
		uint64 ScratchNumElements = Values.Num();
		BeginArray(ElementName, ScratchNumElements);
		Visit(Values.begin(), Values.end());
		EndArray();
	}

	// Sets
	template <typename ElementType, typename KeyFuncs, typename Allocator>
	FORCEINLINE void Visit(const TSet<ElementType, KeyFuncs, Allocator>& Values, const TCHAR* ElementName)
	{
		uint64 ScratchNumElements = Values.Num();
		BeginSet(ElementName, ScratchNumElements);
		for (const auto& Value : Values)
		{
			Visit(const_cast<ElementType&>(Value), TEXT(""));
		}
		EndSet();
	}

	// Maps
	template <typename KeyType, typename ValueType, typename SetAllocator, typename KeyFuncs>
	FORCEINLINE void Visit(TMap<KeyType, ValueType, SetAllocator, KeyFuncs>& Values, const TCHAR* ElementName)
	{
		uint64 ScratchNumElements = Values.Num();
		BeginMap(ElementName, ScratchNumElements);
		for (auto& Kvp : Values)
		{
			BeginObject();
			Visit(Kvp.Key, TEXT("Key"));
			Visit(Kvp.Value, TEXT("Value"));
			EndObject();
		}
		EndMap();
	}

	virtual void ReportNativeBytes(size_t Bytes) {}

	// FArchive support
	virtual FArchive* GetUnderlyingArchive();
	virtual bool IsLoading();
	virtual bool IsTextFormat();

	// Loading support
	virtual FAccessContext GetLoadingContext();

protected:
	FAbstractVisitor() = default;

private:
	FReferrerContext* Context{nullptr};
};

} // namespace Verse
#endif // WITH_VERSE_VM
