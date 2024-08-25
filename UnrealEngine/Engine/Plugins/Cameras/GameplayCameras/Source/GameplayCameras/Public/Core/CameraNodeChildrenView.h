// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "Containers/ArrayView.h"
#include "Misc/TVariant.h"
#include "UObject/ObjectPtr.h"

#include <initializer_list>

class UCameraNode;

/**
 * Structure that describes a list of camera node children.
 *
 * This structure can either provide a TArrayView<> on an existing container of camera node children,
 * or store within itself a list of arbitrary camera node children.
 */
struct FCameraNodeChildrenView
{
public:

	using FArrayView = TArrayView<TObjectPtr<UCameraNode>>;
	using FArray = TArray<TObjectPtr<UCameraNode>, TInlineAllocator<4>>;

	/** An empty view. */
	FCameraNodeChildrenView()
	{
	}

	/** Sets the view to the given TArrayView<>. */
	FCameraNodeChildrenView(FArrayView&& InArrayView)
	{
		Storage.Set<FArrayView>(MoveTemp(InArrayView));
	}

	/** Sets the view to pointer storage and adds the given list of children pointers. */
	FCameraNodeChildrenView(std::initializer_list<TObjectPtr<UCameraNode>> InChildren)
	{
		Storage.Set<FArray>(FArray(InChildren));
	}

	/**
	 * Sets the view to pointer storage (if not already done) and adds the given
	 * pointer to the list.
	 */
	void Add(TObjectPtr<UCameraNode> InChild)
	{
		if (Storage.GetIndex() != FStorage::IndexOfType<FArray>())
		{
			Storage.Set<FArray>(FArray());
		}
		FArray& Array = Storage.Get<FArray>();
		Array.Add(InChild);
	}

	/** Whether this view has any children. */
	bool IsEmpty() const
	{
		switch (Storage.GetIndex())
		{
			case FStorage::IndexOfType<FArrayView>():
				return Storage.Get<FArrayView>().IsEmpty();
			case FStorage::IndexOfType<FArray>():
				return Storage.Get<FArray>().IsEmpty();
			default:
				return true;
		}
	}

	/** Returns the number of children. */
	int32 Num() const
	{
		switch (Storage.GetIndex())
		{
			case FStorage::IndexOfType<FArrayView>():
				return Storage.Get<FArrayView>().Num();
			case FStorage::IndexOfType<FArray>():
				return Storage.Get<FArray>().Num();
			default:
				return 0;
		}
	}

	/** Gets the i'th child. */
	UCameraNode* operator[](int32 Index) const
	{
		switch (Storage.GetIndex())
		{
			case FStorage::IndexOfType<FArrayView>():
				return Storage.Get<FArrayView>()[Index].Get();
			case FStorage::IndexOfType<FArray>():
				return Storage.Get<FArray>()[Index].Get();
			default:
				return nullptr;
		}
	}

public:

	// Range iteration

	struct FBaseIterator
	{
		const FCameraNodeChildrenView* Owner;
		int32 Index;

		FORCEINLINE UCameraNode* operator*()
		{
			return (*Owner)[Index];
		}

		FORCEINLINE bool operator== (const FBaseIterator& Other) const
		{
			return Owner == Other.Owner
				&& Index == Other.Index;
		}

		FORCEINLINE bool operator!= (const FBaseIterator& Other) const
		{
			return !(*this == Other);
		}
	};

	struct FIterator : FBaseIterator
	{
		FORCEINLINE FIterator& operator++()
		{
			++Index;
			return *this;
		}
	};

	FORCEINLINE FIterator begin() const { return FIterator{ this, 0 }; }
	FORCEINLINE FIterator end() const { return FIterator{ this, Num() }; }

	struct FReverseIterator : FBaseIterator
	{
		FORCEINLINE FReverseIterator& operator++()
		{
			--Index;
			return *this;
		}
	};

	FORCEINLINE FReverseIterator rbegin() const { return FReverseIterator{ this, Num() - 1 }; }
	FORCEINLINE FReverseIterator rend() const { return FReverseIterator{ this, -1 }; }

private:

	using FStorage = TVariant<FArrayView, FArray>;
	FStorage Storage;
};

