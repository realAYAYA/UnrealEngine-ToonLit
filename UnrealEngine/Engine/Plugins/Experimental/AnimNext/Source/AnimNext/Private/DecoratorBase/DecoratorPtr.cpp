// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/NodeInstance.h"

namespace UE::AnimNext
{
	FDecoratorPtr::FDecoratorPtr(FNodeInstance* NodeInstance, uint32 DecoratorIndex_)
		: PackedPointerAndFlags(NodeInstance != nullptr ? reinterpret_cast<uintptr_t>(NodeInstance) : 0)
		, DecoratorIndex(static_cast<uint8>(DecoratorIndex_))
	{
		check((reinterpret_cast<uintptr_t>(NodeInstance) & FLAGS_MASK) == 0);	// Make sure we have enough alignment
		check(DecoratorIndex == DecoratorIndex_);	// Make sure we didn't truncate

		if (NodeInstance != nullptr)
		{
			NodeInstance->AddReference();
		}
	}

	FDecoratorPtr::FDecoratorPtr(const FDecoratorPtr& DecoratorPtr)
		: PackedPointerAndFlags(DecoratorPtr.PackedPointerAndFlags)
		, DecoratorIndex(DecoratorPtr.DecoratorIndex)
	{
		// Only increment the reference count if we aren't a weak handle
		// The new handle will remain weak
		if (!DecoratorPtr.IsWeak())
		{
			if (FNodeInstance* Node = DecoratorPtr.GetNodeInstance())
			{
				Node->AddReference();
			}
		}
	}

	FDecoratorPtr::FDecoratorPtr(FDecoratorPtr&& DecoratorPtr) noexcept
		: PackedPointerAndFlags(DecoratorPtr.PackedPointerAndFlags)
		, DecoratorIndex(DecoratorPtr.DecoratorIndex)
	{
		DecoratorPtr.PackedPointerAndFlags = 0;
		DecoratorPtr.DecoratorIndex = 0;
	}

	FDecoratorPtr::FDecoratorPtr(FNodeInstance* NodeInstance, EFlags Flags, uint32 DecoratorIndex_)
		: PackedPointerAndFlags(NodeInstance != nullptr ? (reinterpret_cast<uintptr_t>(NodeInstance) | Flags) : 0)
		, DecoratorIndex(static_cast<uint8>(DecoratorIndex_))
	{
		check((reinterpret_cast<uintptr_t>(NodeInstance) & FLAGS_MASK) == 0);	// Make sure we have enough alignment
		check(DecoratorIndex == DecoratorIndex_);	// Make sure we didn't truncate

		// Only increment the reference count if we aren't a weak handle
		if (NodeInstance != nullptr && (Flags & IS_WEAK_BIT) == 0)
		{
			NodeInstance->AddReference();
		}
	}

	FDecoratorPtr::~FDecoratorPtr()
	{
		Reset();
	}

	FDecoratorPtr& FDecoratorPtr::operator=(const FDecoratorPtr& DecoratorPtr)
	{
		// Add our new reference first in case this == DecoratorPtr

		// Only increment the reference count if we aren't a weak handle
		// The new handle will remain weak
		if (!DecoratorPtr.IsWeak())
		{
			if (FNodeInstance* Node = DecoratorPtr.GetNodeInstance())
			{
				Node->AddReference();
			}
		}

		// Only decrement the reference count if we aren't a weak handle
		if (!IsWeak())
		{
			if (FNodeInstance* Node = GetNodeInstance())
			{
				Node->ReleaseReference();
			}
		}

		PackedPointerAndFlags = DecoratorPtr.PackedPointerAndFlags;
		DecoratorIndex = DecoratorPtr.DecoratorIndex;

		return *this;
	}

	FDecoratorPtr& FDecoratorPtr::operator=(FDecoratorPtr&& DecoratorPtr) noexcept
	{
		Swap(PackedPointerAndFlags, DecoratorPtr.PackedPointerAndFlags);
		Swap(DecoratorIndex, DecoratorPtr.DecoratorIndex);

		return *this;
	}

	void FDecoratorPtr::Reset()
	{
		// Only decrement the reference count if we aren't a weak handle
		if (!IsWeak())
		{
			if (FNodeInstance* Node = GetNodeInstance())
			{
				Node->ReleaseReference();
			}
		}

		PackedPointerAndFlags = 0;
		DecoratorIndex = 0;
	}

	void FWeakDecoratorPtr::Reset()
	{
		NodeInstance = nullptr;
		DecoratorIndex = 0;
	}
}
