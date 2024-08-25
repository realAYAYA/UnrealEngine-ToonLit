// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorPtr.h"

#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorBase/NodeInstance.h"

namespace UE::AnimNext
{
	FDecoratorPtr::FDecoratorPtr(FNodeInstance* InNodeInstance, uint32 InDecoratorIndex)
		: PackedPointerAndFlags(InNodeInstance != nullptr ? reinterpret_cast<uintptr_t>(InNodeInstance) : 0)
		, DecoratorIndex(InDecoratorIndex)
	{
		check((reinterpret_cast<uintptr_t>(InNodeInstance) & FLAGS_MASK) == 0);	// Make sure we have enough alignment
		check(DecoratorIndex <= MAX_uint8);	// Make sure we don't truncate

		if (InNodeInstance != nullptr)
		{
			InNodeInstance->AddReference();
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

	FDecoratorPtr::FDecoratorPtr(FNodeInstance* InNodeInstance, EFlags InFlags, uint32 InDecoratorIndex)
		: PackedPointerAndFlags(InNodeInstance != nullptr ? (reinterpret_cast<uintptr_t>(InNodeInstance) | InFlags) : 0)
		, DecoratorIndex(InDecoratorIndex)
	{
		check((reinterpret_cast<uintptr_t>(InNodeInstance) & FLAGS_MASK) == 0);	// Make sure we have enough alignment
		check(DecoratorIndex <= MAX_uint8);	// Make sure we don't truncate

		// Only increment the reference count if we aren't a weak handle
		if (InNodeInstance != nullptr && (InFlags & IS_WEAK_BIT) == 0)
		{
			InNodeInstance->AddReference();
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
			if (FNodeInstance* NewNode = DecoratorPtr.GetNodeInstance())
			{
				NewNode->AddReference();
			}
		}

		Reset();

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
		// Only decrement the reference count if we aren't a weak handle and if we are valid
		if (!IsWeak())
		{
			if (FNodeInstance* Node = GetNodeInstance())
			{
				FExecutionContext Context(Node->GetOwner());
				Context.ReleaseNodeInstance(*this);
			}
		}

		PackedPointerAndFlags = 0;
		DecoratorIndex = 0;
	}

	FWeakDecoratorPtr::FWeakDecoratorPtr(FNodeInstance* InNodeInstance, uint32 InDecoratorIndex)
		: NodeInstance(InNodeInstance)
		, DecoratorIndex(InDecoratorIndex)
	{
		check(DecoratorIndex <= MAX_uint8);	// Make sure we don't truncate
	}

	void FWeakDecoratorPtr::Reset()
	{
		NodeInstance = nullptr;
		DecoratorIndex = 0;
	}
}
