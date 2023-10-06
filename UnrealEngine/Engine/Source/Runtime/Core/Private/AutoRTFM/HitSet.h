// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TaggedPtr.h"
#include "Utils.h"

namespace AutoRTFM
{
	class FHitSet final
	{
		// TODO: Revisit a good probe depth for the hashset.
		static constexpr uint32_t LinearProbeDepth = 21;

		// TODO: Revisit a good initial capacity for the hitset.
		static constexpr uint32_t InitialCapacity = 128;

	public:
		using Key = TTaggedPtr<void>;

		explicit FHitSet()
		{
			// Do not want to deal with null payloads.
			ASSERT(0 != InitialCapacity);

			// The capacity increases by a power of two so that the modulus is optimal.
			ASSERT(0 == (InitialCapacity & (InitialCapacity - 1)));

			// To make the linear probe check simpler, our payload has a padded
			// linear probe depth so the last element doesn't have to wrap
			// around and insert at the front.
			Payload = new uintptr_t[InitialCapacity + LinearProbeDepth]();
			ASSERT(nullptr != Payload);

			Capacity = InitialCapacity;
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
				case EInsertResult::NeedResize:
					Resize();
					continue;
				}
			}
		}

		bool IsEmpty() const
		{
			return 0 == Size;
		}

		// Clear out the data stored in the set, but does not reduce the capacity.
		void Reset()
		{
			memset(Payload, 0, sizeof(uintptr_t) * Capacity);
			Size = 0;
		}

	private:
		uintptr_t* Payload;

		// TODO: Make this (Capacity - 1) to save a minus in the modulus calculation.
		uint32_t Capacity;

		uint32_t Size;

		void Resize()
		{
			const uintptr_t* const OldPayload = Payload;
			const uint32_t OldCapacity = Capacity;
			const uint32_t OldSize = Size;

			while (true)
			{
				bool NeedAnotherResize = false;

				Size = 0;

				Capacity *= 2;

				// Check that we haven't overflowed our capacity!
				ASSERT(0 != Capacity);

				Payload = new uintptr_t[Capacity + LinearProbeDepth]();
				ASSERT(nullptr != Payload);

				// Now we need to rehash and reinsert all the items. We need to
				// remember about the extra linear probe section here too.
				for (size_t I = 0; I < OldCapacity + LinearProbeDepth; I++)
				{
					// Skip empty locations.
					if (0 == OldPayload[I])
					{
						continue;
					}

					const EInsertResult Result = InsertNoResize(OldPayload[I]);

					if (EInsertResult::NeedResize == Result)
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
			NeedResize
		};

		// Insert something in the HitSet, returning true if the insert
		// succeeded (EG. the key was not already in the set).
		EInsertResult InsertNoResize(const uintptr_t Raw)
		{
			// TODO: Find a great hashing function for the set.
			constexpr uintptr_t A = 0x65d200ce55b19ad8;
			constexpr uintptr_t B = 0x4f2162926e40c299;
			constexpr uintptr_t C = 0x162dd799029970f8;

			const uintptr_t Low = Raw & 0x00000000FFFFFFFF;
			const uintptr_t High = Raw >> 32;

			const uint32_t Hash = static_cast<uint32_t>((A * Low + B * High + C) >> 32);

			if (Size < Capacity)
			{
				const uint32_t HashModCapacity = Hash & (Capacity - 1);

				for (uint32_t D = 0; D < LinearProbeDepth; D++)
				{
					const uintptr_t I = HashModCapacity + D;

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

					// Otherwise we need to keep going with our linear probe...
				}
			}

			return EInsertResult::NeedResize;
		}
	};
}
