// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainerAllocationPolicies.h"
#include "Containers/ContainersFwd.h"
#include "MuR/MemoryTrackingUtils.h"

#include <atomic>
#include <type_traits>

namespace mu
{
	template<typename TagType>
	struct TMemoryCounter
	{
		alignas(8) static inline std::atomic<SSIZE_T> Counter{ 0 };
	};

	template<typename Type>
	struct TIsAMemoryCounter : std::false_type {};

	template<typename TagType>
	struct TIsAMemoryCounter<TMemoryCounter<TagType>> : std::true_type {};

	template<typename BaseAlloc, typename CounterType>
	class FMemoryTrackingAllocatorWrapper
	{
		static_assert(TIsAMemoryCounter<CounterType>::value);

	public:
		using SizeType = typename BaseAlloc::SizeType;

		enum { NeedsElementType = BaseAlloc::NeedsElementType };
		enum { RequireRangeCheck = BaseAlloc::RequireRangeCheck };

		class ForAnyElementType 
		{
		public:
			ForAnyElementType() 
			{
			}

			/** Destructor. */
			FORCEINLINE ~ForAnyElementType()
			{
				CounterType::Counter.fetch_sub(AllocSize, std::memory_order_relaxed);

#if UE_MUTABLE_TRACK_ALLOCATOR_MEMORY_PEAK
				FGlobalMemoryCounter::Update(-AllocSize);
#endif

				AllocSize = 0;
			}

			ForAnyElementType(const ForAnyElementType&) = delete;
			ForAnyElementType& operator=(const ForAnyElementType&) = delete;

			FORCEINLINE decltype(auto) GetAllocation() const
			{
				return Base.GetAllocation();
			}

			FORCEINLINE void MoveToEmpty(ForAnyElementType& Other)
			{
				Base.MoveToEmpty(Other.Base);

				CounterType::Counter.fetch_sub(AllocSize, std::memory_order_relaxed);

#if UE_MUTABLE_TRACK_ALLOCATOR_MEMORY_PEAK
				FGlobalMemoryCounter::Update(-AllocSize);
#endif

				AllocSize = Other.AllocSize;
				Other.AllocSize = 0;
			}

			FORCEINLINE void ResizeAllocation(
				SizeType PreviousNumElements,
				SizeType NumElements,
				SIZE_T NumBytesPerElement
			)
			{
				Base.ResizeAllocation(PreviousNumElements, NumElements, NumBytesPerElement);

				const SSIZE_T Differential = SSIZE_T(NumElements * NumBytesPerElement) - AllocSize;
				const SSIZE_T PrevCounterValue = CounterType::Counter.fetch_add(Differential, std::memory_order_relaxed);
				check(PrevCounterValue >= AllocSize);

#if UE_MUTABLE_TRACK_ALLOCATOR_MEMORY_PEAK
				FGlobalMemoryCounter::Update(Differential);
#endif

				AllocSize = NumElements * NumBytesPerElement;
			}

			FORCEINLINE typename TEnableIf<TAllocatorTraits<BaseAlloc>::SupportsElementAlignment, void>::Type ResizeAllocation(
				SizeType PreviousNumElements,
				SizeType NumElements,
				SIZE_T NumBytesPerElement,
				uint32 AlignmentOfElement
			)
			{
				Base.ResizeAllocation(PreviousNumElements, NumElements, NumBytesPerElement, AlignmentOfElement);

				const SSIZE_T Differential = SSIZE_T(NumElements * NumBytesPerElement) - AllocSize;
				const SSIZE_T PrevCounterValue = CounterType::Counter.fetch_add(Differential, std::memory_order_relaxed);
				check(PrevCounterValue >= AllocSize);

#if UE_MUTABLE_TRACK_ALLOCATOR_MEMORY_PEAK
				FGlobalMemoryCounter::Update(Differential);
#endif

				AllocSize = NumElements * NumBytesPerElement;
			}

			FORCEINLINE SizeType CalculateSlackReserve(
				SizeType NumElements,
				SIZE_T NumBytesPerElement
			) const
			{
				return Base.CalculateSlackReserve(NumElements, NumBytesPerElement);
			}

			FORCEINLINE typename TEnableIf<TAllocatorTraits<BaseAlloc>::SupportsElementAlignment, SizeType>::Type CalculateSlackReserve(
				SizeType NumElements,
				SIZE_T NumBytesPerElement,
				uint32 AlignmentOfElement
			) const
			{
				return Base.CalculateSlackReserve(NumElements, NumBytesPerElement, AlignmentOfElement);
			}

			FORCEINLINE SizeType CalculateSlackShrink(
				SizeType NumElements,
				SizeType CurrentNumSlackElements,
				SIZE_T NumBytesPerElement
			) const
			{
				return Base.CalculateSlackShrink(NumElements, CurrentNumSlackElements, NumBytesPerElement);
			}

			FORCEINLINE typename TEnableIf<TAllocatorTraits<BaseAlloc>::SupportsElementAlignment, SizeType>::Type CalculateSlackShrink(
				SizeType NumElements,
				SizeType CurrentNumSlackElements,
				SIZE_T NumBytesPerElement,
				uint32 AlignmentOfElement
			) const
			{
				return Base.CalculateSlackShrink(NumElements, CurrentNumSlackElements, NumBytesPerElement, AlignmentOfElement);
			}

			SizeType CalculateSlackGrow(
				SizeType NumElements,
				SizeType CurrentNumSlackElements,
				SIZE_T NumBytesPerElement
			) const
			{
				return Base.CalculateSlackGrow(NumElements, CurrentNumSlackElements, NumBytesPerElement);
			}

			FORCEINLINE typename TEnableIf<TAllocatorTraits<BaseAlloc>::SupportsElementAlignment, SizeType>::Type CalculateSlackGrow(
				SizeType NumElements,
				SizeType CurrentNumSlackElements,
				SIZE_T NumBytesPerElement,
				uint32 AlignmentOfElement
			) const
			{
				return Base.CalculateSlackGrow(NumElements, CurrentNumSlackElements, NumBytesPerElement, AlignmentOfElement);
			}

			FORCEINLINE SIZE_T GetAllocatedSize(
				SizeType NumAllocatedElements, 
				SIZE_T NumBytesPerElement
			) const
			{
				return Base.GetAllocatedSize(NumAllocatedElements, NumBytesPerElement);
			}

			FORCEINLINE bool HasAllocation() const
			{
				return Base.HasAllocation();
			}

			FORCEINLINE SizeType GetInitialCapacity() const
			{
				return Base.GetInitialCapacity();
			}

#if UE_ENABLE_ARRAY_SLACK_TRACKING
			FORCEINLINE void SlackTrackerLogNum(SizeType NewNumUsed)
			{
				if constexpr (TAllocatorTraits<BaseAlloc>::SupportsSlackTracking)
				{
					Base.SlackTrackerLogNum(NewNumUsed);
				}
			}
#endif

		private:
			typename BaseAlloc::ForAnyElementType Base;
			SSIZE_T AllocSize = 0;
		};

		template<typename ElementType>
		class ForElementType : public ForAnyElementType
		{
			// Some Allocators, e.g, TAlignedHeapAllocator do some static checks. Instanciate the BaseAlloc::ForElementType
			// at compile time so those warnings are emitted. 
			using BaseTypeInstantaitionType = decltype(DeclVal<typename BaseAlloc::template ForElementType<ElementType>>());

		public:
			ForElementType()
			{
			}

			FORCEINLINE ElementType* GetAllocation() const
			{
				return (ElementType*)ForAnyElementType::GetAllocation();
			}
		};
	};


	// Default memory tarcking allocator needed for TArray and TMap.

	template<typename CounterType>
	using FDefaultMemoryTrackingAllocator = FMemoryTrackingAllocatorWrapper<FDefaultAllocator, CounterType>;

	template<typename CounterType>
	using FDefaultMemoryTrackingBitArrayAllocator = TInlineAllocator<4, FDefaultMemoryTrackingAllocator<CounterType>>;

	template<typename CounterType>
	using FDefaultMemoryTrackingSparceArrayAllocator = TSparseArrayAllocator<
		FDefaultMemoryTrackingAllocator<CounterType>,
		FDefaultMemoryTrackingBitArrayAllocator<CounterType>>;
		
	template<typename CounterType>
	using FDefaultMemoryTrackingSetAllocator = TSetAllocator<
		FDefaultMemoryTrackingSparceArrayAllocator<CounterType>,
		TInlineAllocator<1, FDefaultMemoryTrackingAllocator<CounterType>>,
		DEFAULT_NUMBER_OF_ELEMENTS_PER_HASH_BUCKET,
		DEFAULT_BASE_NUMBER_OF_HASH_BUCKETS,
		DEFAULT_MIN_NUMBER_OF_HASHED_ELEMENTS>;
}

template<typename BaseAlloc, typename Counter>
struct TAllocatorTraits<mu::FMemoryTrackingAllocatorWrapper<BaseAlloc, Counter>> : public TAllocatorTraitsBase<mu::FMemoryTrackingAllocatorWrapper<BaseAlloc, Counter>>
{
	enum { SupportsElementAlignment = TAllocatorTraits<BaseAlloc>::SupportsElementAlignment };
	enum { SupportsSlackTracking = TAllocatorTraits<BaseAlloc>::SupportsSlackTracking };
	enum { SupportsMoveFromOtherAllocator = false };
};


