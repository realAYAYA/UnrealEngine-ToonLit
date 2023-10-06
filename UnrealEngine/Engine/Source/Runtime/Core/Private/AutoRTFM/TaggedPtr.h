// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Utils.h"
#include <stdint.h>

namespace AutoRTFM
{
	// TODO: Could get lower bits too due to alignment?
	// TODO: Do we need to care about 5-level paging and their 57-bit pointers?
	template<typename T, typename TopTagT = uint16_t> class TTaggedPtr final
	{
		// Our tagged pointer assumes 64-bit pointers.
		static_assert(sizeof(uintptr_t) == sizeof(uint64_t));

		// For our tagged pointers we assume the bottom 48-bits are used, and
		// so the top 16-bits are free for our tag.
		static_assert(sizeof(TopTagT) == sizeof(uint16_t));

		static constexpr unsigned PointerBits = 48;
		static constexpr uintptr_t PointerMask = (static_cast<uintptr_t>(1) << PointerBits) - 1;
		static constexpr unsigned TopTagBits = sizeof(TopTagT);
		static constexpr uintptr_t TopTagMask = (static_cast<uintptr_t>(1) << TopTagBits) - 1;

	public:
		struct Hasher final
		{
		    size_t operator()(const TTaggedPtr<T, TopTagT>& TaggedPtr) const
		    {
		      return TaggedPtr.Payload;
		    }
		};

		// Assumes little-endian, so top tag is the last 2-bytes in memory.
		static constexpr size_t OffsetOfTopTag = PointerBits / 8;

		TTaggedPtr()
		{
			Reset();
		}

		explicit TTaggedPtr(T* const Pointer) : Payload(reinterpret_cast<uintptr_t>(Pointer))
		{
			ASSERT(0 == (Payload & ~PointerMask));
		}

		explicit TTaggedPtr(const TTaggedPtr& Other) : Payload(Other.Payload) {}
		explicit TTaggedPtr(const TTaggedPtr&& Other) : Payload(Other.Payload) {}

		TTaggedPtr& operator=(T* const Pointer)
		{
			const uintptr_t NewPayload = reinterpret_cast<uintptr_t>(Pointer);

			ASSERT(0 == (NewPayload & ~PointerMask));
			Payload &= ~PointerMask;
			Payload |= NewPayload;
			return *this;
		}

		TTaggedPtr& operator=(TTaggedPtr& Other)
		{
			Payload = Other.Payload;
			return *this;
		}

		TTaggedPtr& operator=(TTaggedPtr&& Other)
		{
			Payload = Other.Payload;
			return *this;
		}

		T& operator*() const
	    {
	    	return *reinterpret_cast<T*>(Payload & PointerMask);
	    }

	    T* operator->() const
	    {
	    	return reinterpret_cast<T*>(Payload & PointerMask);
	    }

	    T* Get() const
	    {
	    	return reinterpret_cast<T*>(Payload & PointerMask);
	    }

	    void SetTopTag(const TopTagT Tag)
	    {
	    	*reinterpret_cast<TopTagT*>(reinterpret_cast<uint8_t*>(&Payload) + OffsetOfTopTag) = MoveTemp(Tag);
	    }

	    TopTagT GetTopTag() const
	    {
	    	return *reinterpret_cast<const TopTagT*>(reinterpret_cast<const uint8_t*>(&Payload) + OffsetOfTopTag);
	    }

	    void Reset()
	    {
	    	Payload = 0;
	    }

	    uintptr_t GetPayload() const
	    {
	    	return Payload;
	    }

	private:
		uintptr_t Payload;
	};

	template<typename TopTagT> class TTaggedPtr<void, TopTagT> final
	{
		// Our tagged pointer assumes 64-bit pointers.
		static_assert(sizeof(uintptr_t) == sizeof(uint64_t));

		// For our tagged pointers we assume the bottom 48-bits are used, and
		// so the top 16-bits are free for our tag.
		static_assert(sizeof(TopTagT) == sizeof(uint16_t));

		static constexpr unsigned PointerBits = 48;
		static constexpr uintptr_t PointerMask = (static_cast<uintptr_t>(1) << PointerBits) - 1;
		static constexpr unsigned TopTagBits = sizeof(TopTagT);
		static constexpr uintptr_t TopTagMask = (static_cast<uintptr_t>(1) << TopTagBits) - 1;

	public:
		struct Hasher final
		{
		    size_t operator()(const TTaggedPtr<void, TopTagT>& TaggedPtr) const
		    {
		      return TaggedPtr.Payload;
		    }
		};

		// Assumes little-endian, so top tag is the last 2-bytes in memory.
		static constexpr size_t OffsetOfTopTag = PointerBits / 8;

		TTaggedPtr()
		{
			Reset();
		}

		TTaggedPtr(void* const Pointer) : Payload(reinterpret_cast<uintptr_t>(Pointer))
		{
			ASSERT(0 == (Payload & ~PointerMask));
		}

		TTaggedPtr(const TTaggedPtr& Other) : Payload(Other.Payload) {}
		TTaggedPtr(const TTaggedPtr&& Other) : Payload(Other.Payload) {}

		TTaggedPtr& operator=(void* const Pointer)
		{
			const uintptr_t NewPayload = reinterpret_cast<uintptr_t>(Pointer);

			ASSERT(0 == (NewPayload & ~PointerMask));
			Payload &= ~PointerMask;
			Payload |= NewPayload;
			return *this;
		}

		TTaggedPtr& operator=(TTaggedPtr& Other)
		{
			Payload = Other.Payload;
			return *this;
		}

		TTaggedPtr& operator=(TTaggedPtr&& Other)
		{
			Payload = Other.Payload;
			return *this;
		}

		bool operator==(const TTaggedPtr& Other) const
		{
			return Payload == Other.Payload;
		}

		void* Get() const
	    {
	    	return reinterpret_cast<void*>(Payload & PointerMask);
	    }

	    void SetTopTag(const TopTagT Tag)
	    {
	    	*reinterpret_cast<TopTagT*>(reinterpret_cast<uint8_t*>(&Payload) + OffsetOfTopTag) = Tag;
	    }

	    TopTagT GetTopTag() const
	    {
	    	return *reinterpret_cast<const TopTagT*>(reinterpret_cast<const uint8_t*>(&Payload) + OffsetOfTopTag);
	    }

	    void Reset()
	    {
	    	Payload = 0;
	    }

	    uintptr_t GetPayload() const
	    {
	    	return Payload;
	    }

	private:
		uintptr_t Payload;
	};
}
