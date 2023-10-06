// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Templates/SharedPointer.h"
#include "MVVM/ViewModelTypeID.h"
#include "MVVM/ViewModelPtr.h"


namespace UE::Sequencer
{

class FViewModel;
class FSequencerCoreSelection;

template<typename MixinType, typename KeyType>
class TSelectionSetBase;

template<typename StorageKeyType, typename FilterType>
struct TFilteredViewModelSelectionIterator;

/**
 * Minimal traits class for iterator implementations.
 * Can be specialized to customize selection set iteration behavior.
 */
template<typename T>
struct TSelectionSetIteratorImpl
{
	/**
	 * Access the current value of the iterator. By default this just returns the value directly.
	 */
	const T& operator*() const
	{
		return *Iterator;
	}

protected:

	explicit TSelectionSetIteratorImpl(typename TSet<T>::TRangedForConstIterator InIterator)
		: Iterator(InIterator)
	{}

	/**
	 * Skip invalid iterations - by default all items are valid
	 */
	constexpr void SkipInvalid()
	{}

	typename TSet<T>::TRangedForConstIterator Iterator;
};

/**
 * Selection iterator class that iterates a given selection set using specialized behavior for weak pointers
 */
template<typename T, typename ImplType = TSelectionSetIteratorImpl<T>>
struct TSelectionSetIteratorState : ImplType
{
	explicit TSelectionSetIteratorState(typename TSet<T>::TRangedForConstIterator InIterator)
		: ImplType(InIterator)
	{
		this->SkipInvalid();
	}

	void operator++()
	{
		++this->Iterator;
		this->SkipInvalid();
	}

	explicit operator bool() const
	{
		return static_cast<bool>(this->Iterator);
	}

	friend bool operator!=(const TSelectionSetIteratorState<T, ImplType>& A, const TSelectionSetIteratorState<T, ImplType>& B)
	{
		return A.Iterator != B.Iterator;
	}
};


template<typename KeyType, typename FilterType>
struct TFilteredSelectionSetIteratorImpl
{
	/**
	 * Access the current value of the iterator. By default this just returns the value directly.
	 */
	TViewModelPtr<FilterType> operator*() const
	{
		return CurrentValue;
	}

protected:

	explicit TFilteredSelectionSetIteratorImpl(typename TSet<KeyType>::TRangedForConstIterator InIterator)
		: Iterator(InIterator)
	{}

	void SkipInvalid()
	{
		CurrentValue = nullptr;
		while (Iterator)
		{
			CurrentValue = CastViewModel<FilterType>(Iterator->Pin());
			if (CurrentValue)
			{
				break;
			}
			++Iterator;
		}
	}

	typename TSet<KeyType>::TRangedForConstIterator Iterator;
	TViewModelPtr<FilterType> CurrentValue;
};

/**
 * Filtered iterator for a TSelectionSet<TWeakViewModelPtr<T>> that iterates only the items of a specific filtered type
 */
template<typename StorageKeyType, typename FilterType>
using TFilteredViewModelSelectionIteratorState = TSelectionSetIteratorState<StorageKeyType, TFilteredSelectionSetIteratorImpl<StorageKeyType, FilterType>>;

/**
 * Iterator for iterating unique fragment selection sets
 */
template<typename KeyType>
struct TUniqueFragmentSelectionSetIterator
{
	void operator++()
	{
		++Iterator;
	}

	explicit operator bool() const
	{
		return static_cast<bool>(Iterator);
	}

	operator KeyType() const
	{
		return *Iterator;
	}

	KeyType operator*() const
	{
		return *Iterator;
	}

	friend bool operator!=(const TUniqueFragmentSelectionSetIterator<KeyType>& A, const TUniqueFragmentSelectionSetIterator<KeyType>& B)
	{
		return A.Iterator != B.Iterator;
	}

private:

	template<typename, typename>
	friend class TUniqueFragmentSelectionSet;

	explicit TUniqueFragmentSelectionSetIterator(typename TSet<KeyType>::TRangedForConstIterator InIterator)
		: Iterator(InIterator)
	{}

	typename TSet<KeyType>::TRangedForConstIterator Iterator;
};

template<typename StorageKeyType, typename FilterType>
struct TFilteredViewModelSelectionIterator
{
	const TSet<StorageKeyType>* SelectionSet;

	FORCEINLINE TFilteredViewModelSelectionIteratorState<StorageKeyType, FilterType> begin() const { return TFilteredViewModelSelectionIteratorState<StorageKeyType, FilterType>(SelectionSet->begin()); }
	FORCEINLINE TFilteredViewModelSelectionIteratorState<StorageKeyType, FilterType> end() const   { return TFilteredViewModelSelectionIteratorState<StorageKeyType, FilterType>(SelectionSet->end());   }
};


/**
 * Specialization for TWeakPtr types that skips invalid pointers and exposes iterators as TSharedPtr
 */
template<typename T>
struct TSelectionSetIteratorImpl<TWeakPtr<T>>
{
	TSharedPtr<T> operator*() const
	{
		return Iterator->Pin();
	}

protected:

	explicit TSelectionSetIteratorImpl(typename TSet<TWeakPtr<T>>::TRangedForConstIterator InIterator)
		: Iterator(InIterator)
	{}

	void SkipInvalid()
	{
		while (Iterator && !Iterator->Pin())
		{
			++Iterator;
		}
	}

	typename TSet<TWeakPtr<T>>::TRangedForConstIterator Iterator;
};


/**
 * Specialization for TWeakViewModelPtr types that skips invalid pointers and exposes iterators as TViewModelPtr
 */
template<typename T>
struct TSelectionSetIteratorImpl<TWeakViewModelPtr<T>>
{
	TViewModelPtr<T> operator*() const
	{
		return Iterator->Pin();
	}

protected:

	explicit TSelectionSetIteratorImpl(typename TSet<TWeakViewModelPtr<T>>::TRangedForConstIterator InIterator)
		: Iterator(InIterator)
	{}

	void SkipInvalid()
	{
		while (Iterator && !Iterator->Pin())
		{
			++Iterator;
		}
	}

	typename TSet<TWeakViewModelPtr<T>>::TRangedForConstIterator Iterator;
};


} // namespace UE::Sequencer