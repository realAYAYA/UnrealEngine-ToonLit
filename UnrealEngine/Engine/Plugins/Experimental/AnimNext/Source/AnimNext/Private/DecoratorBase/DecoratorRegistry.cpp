// Copyright Epic Games, Inc. All Rights Reserved.

#include "DecoratorBase/DecoratorRegistry.h"

#include "DecoratorBase/Decorator.h"

namespace UE::AnimNext
{
	namespace Private
	{
		static FDecoratorRegistry* GDecoratorRegistry = nullptr;

		static TArray<DecoratorConstructorFunc> GPendingRegistrationQueue;
	}

	FDecoratorRegistry& FDecoratorRegistry::Get()
	{
		checkf(Private::GDecoratorRegistry, TEXT("Decorator Registry is not instanced. It is only valid to access this while the engine module is loaded."));
		return *Private::GDecoratorRegistry;
	}

	void FDecoratorRegistry::Init()
	{
		if (ensure(Private::GDecoratorRegistry == nullptr))
		{
			Private::GDecoratorRegistry = new FDecoratorRegistry();

			// Register all our pending static init decorators
			for (DecoratorConstructorFunc DecoratorConstructor : Private::GPendingRegistrationQueue)
			{
				Private::GDecoratorRegistry->AutoRegisterImpl(DecoratorConstructor);
			}

			// Reset the registration queue, it won't be used anymore now that the registry is up and ready
			Private::GPendingRegistrationQueue.Empty(0);
		}
	}

	void FDecoratorRegistry::Destroy()
	{
		if (ensure(Private::GDecoratorRegistry != nullptr))
		{
			TArray<FRegistryEntry> Entries;
			Private::GDecoratorRegistry->DecoratorUIDToEntryMap.GenerateValueArray(Entries);

			for (const FRegistryEntry& Entry : Entries)
			{
				Private::GDecoratorRegistry->Unregister(Entry.Decorator);
			}

			delete Private::GDecoratorRegistry;
			Private::GDecoratorRegistry = nullptr;
		}
	}

	void FDecoratorRegistry::StaticRegister(DecoratorConstructorFunc DecoratorConstructor)
	{
		if (Private::GDecoratorRegistry != nullptr)
		{
			// Registry is already up and running, use it
			Private::GDecoratorRegistry->AutoRegisterImpl(DecoratorConstructor);
		}
		else
		{
			// Registry isn't ready yet, queue up our decorator
			// Once Init() is called, our queue will be processed
			Private::GPendingRegistrationQueue.Add(DecoratorConstructor);
		}
	}

	void FDecoratorRegistry::StaticUnregister(DecoratorConstructorFunc DecoratorConstructor)
	{
		if (Private::GDecoratorRegistry != nullptr)
		{
			// Registry is already up and running, use it
			Private::GDecoratorRegistry->AutoUnregisterImpl(DecoratorConstructor);
		}
		else
		{
			// Registry isn't ready yet or it got destroyed before the decorators are unregistering
			const int32 DecoratorIndex = Private::GPendingRegistrationQueue.IndexOfByKey(DecoratorConstructor);
			if (DecoratorIndex != INDEX_NONE)
			{
				Private::GPendingRegistrationQueue.RemoveAtSwap(DecoratorIndex);
			}
		}
	}

	void FDecoratorRegistry::AutoRegisterImpl(DecoratorConstructorFunc DecoratorConstructor)
	{
		// Grab the memory requirements of our decorator
		FDecoratorMemoryLayout DecoratorMemoryRequirements;
		DecoratorConstructor(nullptr, DecoratorMemoryRequirements);

		// Align it and reserve space
		const uint32 OldBufferOffset = StaticDecoratorBufferOffset;

		uint8* DecoratorPtr = Align(&StaticDecoratorBuffer[OldBufferOffset], DecoratorMemoryRequirements.DecoratorAlignment);

		const uint32 NewBufferOffset = (DecoratorPtr + DecoratorMemoryRequirements.DecoratorSize) - &StaticDecoratorBuffer[0];
		const bool bFitsInStaticBuffer = NewBufferOffset <= STATIC_DECORATOR_BUFFER_SIZE;

		FDecoratorRegistryHandle DecoratorHandle;
		if (bFitsInStaticBuffer)
		{
			// This decorator fits in our buffer, add it
			StaticDecoratorBufferOffset = NewBufferOffset;
			DecoratorHandle = FDecoratorRegistryHandle::MakeStatic(DecoratorPtr - &StaticDecoratorBuffer[0]);
		}
		else
		{
			// We have too many static decorators, we should consider increasing the static buffer
			// TODO: Warn
			// Allocate the decorator on the heap instead
			DecoratorPtr = static_cast<uint8*>(FMemory::Malloc(DecoratorMemoryRequirements.DecoratorSize, DecoratorMemoryRequirements.DecoratorAlignment));
		}

		FDecorator* Decorator = DecoratorConstructor(DecoratorPtr, DecoratorMemoryRequirements);
		checkf((void*)DecoratorPtr == (void*)Decorator, TEXT("FDecorator 'this' should be where we specified"));

		const FDecoratorUID DecoratorUID = Decorator->GetDecoratorUID();

		if (ensure(!DecoratorUIDToEntryMap.Contains(DecoratorUID.GetUID())))
		{
			// This is a new decorator, we'll keep it
			if (!bFitsInStaticBuffer)
			{
				// Find our dynamic decorator index
				int32 DecoratorIndex;
				if (DynamicDecoratorFreeIndexHead != INDEX_NONE)
				{
					// We already had a free index, grab it
					DecoratorIndex = DynamicDecoratorFreeIndexHead;
					DynamicDecoratorFreeIndexHead = DynamicDecorators[DynamicDecoratorFreeIndexHead];
				}
				else
				{
					// No free indices, allocate a new one
					DecoratorIndex = DynamicDecorators.Add(reinterpret_cast<uintptr_t>(Decorator));
				}

				DecoratorHandle = FDecoratorRegistryHandle::MakeDynamic(DecoratorIndex);
			}

			DecoratorUIDToEntryMap.Add(DecoratorUID.GetUID(), FRegistryEntry{ Decorator, DecoratorConstructor, DecoratorHandle });
		}
		else
		{
			// We have already registered this decorator, destroy our temporary instance
			Decorator->~FDecorator();

			if (bFitsInStaticBuffer)
			{
				// We were in the static buffer, clear our entry
				StaticDecoratorBufferOffset = OldBufferOffset;
				FMemory::Memzero(&StaticDecoratorBuffer[OldBufferOffset], NewBufferOffset - OldBufferOffset);
			}
			else
			{
				// It isn't in the static buffer, free it
				FMemory::Free(Decorator);
				Decorator = nullptr;
			}
		}
	}

	void FDecoratorRegistry::AutoUnregisterImpl(DecoratorConstructorFunc DecoratorConstructor)
	{
		for (auto It = DecoratorUIDToEntryMap.CreateIterator(); It; ++It)
		{
			FRegistryEntry& Entry = It.Value();

			if (Entry.DecoratorConstructor == DecoratorConstructor)
			{
				check(Entry.DecoratorHandle.IsValid());

				// Destroy and release our decorator
				// We always own auto-registered decorator instances
				Entry.Decorator->~FDecorator();

				if (Entry.DecoratorHandle.IsDynamic())
				{
					// It has been dynamically registered, free it
					const int32 DecoratorIndex = Entry.DecoratorHandle.GetDynamicIndex();

					DynamicDecorators[DecoratorIndex] = DynamicDecoratorFreeIndexHead;
					DynamicDecoratorFreeIndexHead = DecoratorIndex;

					FMemory::Free(Entry.Decorator);
				}
				else
				{
					// It was in the static buffer, we cannot reclaim the space easily, we'd have to add metadata to
					// track unused chunks so that we could coalesce
					if (DecoratorUIDToEntryMap.Num() == 1)
					{
						// Last static decorator is being removed, reclaims all the space
						StaticDecoratorBufferOffset = 0;
					}
				}

				DecoratorUIDToEntryMap.Remove(It.Key());

				break;
			}
		}
	}

	FDecoratorRegistryHandle FDecoratorRegistry::FindHandle(FDecoratorUID DecoratorUID) const
	{
		if (!DecoratorUID.IsValid())
		{
			return FDecoratorRegistryHandle();
		}

		if (const FRegistryEntry* Entry = DecoratorUIDToEntryMap.Find(DecoratorUID.GetUID()))
		{
			return Entry->DecoratorHandle;
		}

		// Decorator not found
		return FDecoratorRegistryHandle();
	}

	const FDecorator* FDecoratorRegistry::Find(FDecoratorRegistryHandle DecoratorHandle) const
	{
		if (!DecoratorHandle.IsValid())
		{
			return nullptr;
		}

		if (DecoratorHandle.IsStatic())
		{
			const int32 DecoratorOffset = DecoratorHandle.GetStaticOffset();
			return reinterpret_cast<const FDecorator*>(&StaticDecoratorBuffer[DecoratorOffset]);
		}
		else
		{
			const int32 DecoratorIndex = DecoratorHandle.GetDynamicIndex();
			return reinterpret_cast<const FDecorator*>(DynamicDecorators[DecoratorIndex]);
		}
	}

	const FDecorator* FDecoratorRegistry::Find(FDecoratorUID DecoratorUID) const
	{
		const FDecoratorRegistryHandle DecoratorHandle = FindHandle(DecoratorUID);
		return Find(DecoratorHandle);
	}

	const FDecorator* FDecoratorRegistry::Find(const UScriptStruct* DecoratorSharedDataStruct) const
	{
		if (DecoratorSharedDataStruct == nullptr)
		{
			return nullptr;
		}

		for (const auto& KeyValuePair : DecoratorUIDToEntryMap)
		{
			const FDecorator* Decorator = KeyValuePair.Value.Decorator;
			if (Decorator->GetDecoratorSharedDataStruct() == DecoratorSharedDataStruct)
			{
				return Decorator;
			}
		}

		return nullptr;
	}

	void FDecoratorRegistry::Register(FDecorator* Decorator)
	{
		if (Decorator == nullptr)
		{
			return;
		}

		const FDecoratorUID DecoratorUID = Decorator->GetDecoratorUID();

		if (ensure(!DecoratorUIDToEntryMap.Contains(DecoratorUID.GetUID())))
		{
			// This is a new decorator, we'll keep it
			// Find our dynamic decorator index
			int32 DecoratorIndex;
			if (DynamicDecoratorFreeIndexHead != INDEX_NONE)
			{
				// We already had a free index, grab it
				DecoratorIndex = DynamicDecoratorFreeIndexHead;
				DynamicDecoratorFreeIndexHead = DynamicDecorators[DynamicDecoratorFreeIndexHead];

				DynamicDecorators[DecoratorIndex] = reinterpret_cast<uintptr_t>(Decorator);
			}
			else
			{
				// No free indices, allocate a new one
				DecoratorIndex = DynamicDecorators.Add(reinterpret_cast<uintptr_t>(Decorator));
			}

			FDecoratorRegistryHandle DecoratorHandle = FDecoratorRegistryHandle::MakeDynamic(DecoratorIndex);

			DecoratorUIDToEntryMap.Add(DecoratorUID.GetUID(), FRegistryEntry{ Decorator, nullptr, DecoratorHandle });
		}
	}

	void FDecoratorRegistry::Unregister(FDecorator* Decorator)
	{
		if (Decorator == nullptr)
		{
			return;
		}

		const FDecoratorUID DecoratorUID = Decorator->GetDecoratorUID();

		if (FRegistryEntry* Entry = DecoratorUIDToEntryMap.Find(DecoratorUID.GetUID()))
		{
			check(Entry->DecoratorHandle.IsValid());

			if (Entry->DecoratorHandle.IsDynamic())
			{
				// It has been dynamically registered, free it
				const int32 DecoratorIndex = Entry->DecoratorHandle.GetDynamicIndex();

				DynamicDecorators[DecoratorIndex] = DynamicDecoratorFreeIndexHead;
				DynamicDecoratorFreeIndexHead = DecoratorIndex;
			}

			if (Entry->DecoratorConstructor != nullptr)
			{
				// We own this decorator instance, destroy it
				Entry->Decorator->~FDecorator();

				if (Entry->DecoratorHandle.IsDynamic())
				{
					FMemory::Free(Entry->Decorator);
				}
				else
				{
					// It was in the static buffer, we cannot reclaim the space
				}
			}

			DecoratorUIDToEntryMap.Remove(DecoratorUID.GetUID());
		}
	}

	TArray<const FDecorator*> FDecoratorRegistry::GetDecorators() const
	{
		TArray<const FDecorator*> Decorators;
		Decorators.Reserve(DecoratorUIDToEntryMap.Num());
		
		for (const auto& It : DecoratorUIDToEntryMap)
		{
			Decorators.Add(It.Value.Decorator);
		}

		return Decorators;
	}

	uint32 FDecoratorRegistry::GetNum() const
	{
		return DecoratorUIDToEntryMap.Num();
	}
}
