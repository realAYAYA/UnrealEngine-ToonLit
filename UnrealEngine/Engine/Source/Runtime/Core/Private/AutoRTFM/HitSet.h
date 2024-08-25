// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TaggedPtr.h"
#include "Utils.h"

namespace AutoRTFM
{
	class FHitSet final
	{
		// TODO: Revisit a good probe depth for the hashset.
		static constexpr uint32_t LinearProbeDepth = 16;

		// TODO: Revisit a good initial capacity for the hitset.
		static constexpr uint32_t LogInitialCapacity = 4;

	public:
		using Key = TTaggedPtr<void>;

		explicit FHitSet()
		{
			constexpr uint32_t InitialCapacity = 1u << LogInitialCapacity;

			// Do not want to deal with null payloads.
			static_assert(0 != InitialCapacity);

			// The capacity is always a power of two so that the range reduction is optimal.
			static_assert(0 == (InitialCapacity & (InitialCapacity - 1)));

			Payload = new uintptr_t[InitialCapacity]();
			ASSERT(nullptr != Payload);

			SixtyFourMinusLogCapacity = 64 - LogInitialCapacity;
			Size = 0;
		}

		~FHitSet()
		{
			delete[] Payload;
		}

		// Insert something in the HitSet, returning true if the put succeeded
		// (EG. the key was not already in the set).
		bool Insert(Key K)
		{
			while (true)
			{
				switch (InsertNoResize(K.GetPayload()))
				{
				default:
					AutoRTFM::Unreachable();
				case EInsertResult::Inserted:
					return true;
				case EInsertResult::Exists:
					return false;
				case EInsertResult::NotInserted:
					Resize();
					continue;
				}
			}
		}

		UE_AUTORTFM_FORCEINLINE bool IsEmpty() const
		{
			return 0 == Size;
		}

		// Clear out the data stored in the set, but does not reduce the capacity.
		void Reset()
		{
			memset(Payload, 0, sizeof(uintptr_t) * Capacity());
			Size = 0;
		}

		uint32_t GetCapacity() const
		{
			return Capacity();
		}

		uint32_t GetSize() const
		{
			return Size;
		}

	private:
		uintptr_t* Payload;

		uint32_t SixtyFourMinusLogCapacity;

		uint32_t Size;

		UE_AUTORTFM_FORCEINLINE uint32_t Capacity() const
		{
			return 1u << (64 - SixtyFourMinusLogCapacity);
		}

		UE_AUTORTFM_FORCEINLINE void IncreaseCapacity()
		{
			// It seems odd - but subtracting 1 here is totally intentional
			// because of how we store our capacity as (64 - log2(Capacity)).
			SixtyFourMinusLogCapacity -= 1;

			// Check that we haven't overflowed our capacity!
			ASSERT(0 != SixtyFourMinusLogCapacity);
		}

		void Resize()
		{
			const uintptr_t* const OldPayload = Payload;
			const uint32_t OldCapacity = Capacity();
			const uint32_t OldSize = Size;

			while (true)
			{
				bool NeedAnotherResize = false;

				Size = 0;

				IncreaseCapacity();

				Payload = new uintptr_t[Capacity()]();
				ASSERT(nullptr != Payload);

				// Now we need to rehash and reinsert all the items. We need to
				// remember about the extra linear probe section here too.
				for (size_t I = 0; I < OldCapacity; I++)
				{
					// Skip empty locations.
					if (0 == OldPayload[I])
					{
						continue;
					}

					const EInsertResult Result = InsertNoResize(OldPayload[I]);

					if (EInsertResult::NotInserted == Result)
					{
						NeedAnotherResize = true;
						break;
					}
					else
					{
						ASSERT(EInsertResult::Inserted == Result);
					}
				}

				if (NeedAnotherResize)
				{
					delete[] Payload;
					continue;
				}

				break;
			}

			ASSERT(OldSize == Size);

			delete[] OldPayload;

			return;
		}

		enum class EInsertResult : uint8_t
		{
			Exists,
			Inserted,
			NotInserted
		};

		UE_AUTORTFM_FORCEINLINE uintptr_t FirstHashInRange(const uintptr_t Hashee) const
		{
			constexpr uintptr_t Fibonacci = 0x9E3779B97F4A7C15;
			return (Hashee * Fibonacci) >> SixtyFourMinusLogCapacity;
		}

		UE_AUTORTFM_FORCEINLINE uintptr_t SecondHash(const uintptr_t Hashee) const
		{
			// TODO: Find a great hashing function for the set.
			constexpr uintptr_t A = 0x65d200ce55b19ad8;
			constexpr uintptr_t B = 0x4f2162926e40c299;
			constexpr uintptr_t C = 0x162dd799029970f8;

			const uintptr_t Low = Hashee & 0x00000000FFFFFFFF;
			const uintptr_t High = Hashee >> 32;

			const uintptr_t Hash = A * Low + B * High + C;

			return Hash;
		}

		UE_AUTORTFM_FORCEINLINE EInsertResult TryInsertAtIndex(const uintptr_t Raw, const uintptr_t I)
		{
			// We have a free location in the set.
			if (0 == Payload[I])
			{
				Payload[I] = Raw;
				Size++;
				return EInsertResult::Inserted;
			}

			// We're already in the set.
			if (Raw == Payload[I])
			{
				return EInsertResult::Exists;
			}

			return EInsertResult::NotInserted;
		}

		// Insert something in the HitSet, returning true if the insert
		// succeeded (EG. the key was not already in the set).
		UE_AUTORTFM_FORCEINLINE EInsertResult InsertNoResize(const uintptr_t Raw)
		{
			// First try to use our simple first hash to get a location.
			{
				const uintptr_t I = FirstHashInRange(Raw);

				const EInsertResult R = TryInsertAtIndex(Raw, I);

				if (EInsertResult::NotInserted != R)
				{
					return R;
				}
			}

			// Then, if that fails, we use a second more complicated hash with a linear probe.
			const uintptr_t Hash = SecondHash(Raw);

			for (uint32_t D = 0; D < LinearProbeDepth; D++)
			{
				const uintptr_t I = FirstHashInRange(Hash + D);

				const EInsertResult R = TryInsertAtIndex(Raw, I);

				if (EInsertResult::NotInserted != R)
				{
					return R;
				}

				// Otherwise we need to keep going with our linear probe...
			}

			return EInsertResult::NotInserted;
		}
	};
}
