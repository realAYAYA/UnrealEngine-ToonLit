// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "HAL/Platform.h"
#include "CoreTypes.h"
#include "Traits/IsContiguousContainer.h"

/// @cond DOXYGEN_WARNINGS
template<int IndexSize> class TSizedDefaultAllocator;
using FDefaultAllocator = TSizedDefaultAllocator<32>;
using FDefaultAllocator64 = TSizedDefaultAllocator<64>;
class FDefaultSetAllocator;

class FString;
class FAnsiString;
class FUtf8String;

template<> struct TIsContiguousContainer<FString>     { static constexpr bool Value = true; };
template<> struct TIsContiguousContainer<FAnsiString> { static constexpr bool Value = true; };
template<> struct TIsContiguousContainer<FUtf8String> { static constexpr bool Value = true; };

template<typename T, typename Allocator = FDefaultAllocator> class TArray;
template<typename T> using TArray64 = TArray<T, FDefaultAllocator64>;
template<typename T, typename SizeType = int32> class TArrayView;
template<typename T> using TArrayView64 = TArrayView<T, int64>;
template<typename T, typename SizeType = int32> using TConstArrayView = TArrayView<const T, SizeType>;
template<typename T> using TConstArrayView64 = TConstArrayView<T, int64>;
template<typename T> class TTransArray;
template<typename InKeyType, typename InValueType, bool bInAllowDuplicateKeys> struct TDefaultMapHashableKeyFuncs;
template<typename InKeyType, typename InValueType, typename SetAllocator = FDefaultSetAllocator, typename KeyFuncs = TDefaultMapHashableKeyFuncs<InKeyType, InValueType, false> > class TMap;
template<typename KeyType, typename ValueType, typename SetAllocator = FDefaultSetAllocator, typename KeyFuncs = TDefaultMapHashableKeyFuncs<KeyType, ValueType, true > > class TMultiMap;
template <typename T = void > struct TLess;
template <typename> struct TTypeTraits;
template<typename InKeyType, typename InValueType, typename ArrayAllocator = FDefaultAllocator, typename SortPredicate = TLess<typename TTypeTraits<InKeyType>::ConstPointerType> > class TSortedMap;
template<typename ElementType,bool bInAllowDuplicateKeys = false> struct DefaultKeyFuncs;
template<typename InElementType, typename KeyFuncs = DefaultKeyFuncs<InElementType>, typename Allocator = FDefaultSetAllocator> class TSet;
template<typename InElementType, typename InSizeType = int32> class TStridedView;
template<typename T, typename SizeType = int32> using TConstStridedView = TStridedView<const T, SizeType>;
/// @endcond
