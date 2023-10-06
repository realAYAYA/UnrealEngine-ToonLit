// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/Decorator.h"
#include "DecoratorBase/DecoratorUID.h"
#include "DecoratorBase/DecoratorRegistryHandle.h"

namespace UE::AnimNext
{
	/**
	 * FDecoratorRegistry
	 * 
	 * A global registry of all existing decorators that can be used in animation graphs.
	 * 
	 * @see FDecorator
	 */
	struct ANIMNEXT_API FDecoratorRegistry final
	{
		// Access the global registry
		static FDecoratorRegistry& Get();

		// Finds and returns the decorator handle for the provided decorator UID or an invalid
		// handle if that decorator hasn't been registered yet.
		FDecoratorRegistryHandle FindHandle(uint32 DecoratorUID) const;

		// Finds and returns the decorator handle for the provided decorator UID or an invalid
		// handle if that decorator hasn't been registered yet.
		FDecoratorRegistryHandle FindHandle(FDecoratorUID DecoratorUID) const;

		// Finds and returns the decorator associated with the provided handle. If the handle
		// is invalid, nullptr is returned.
		const FDecorator* Find(FDecoratorRegistryHandle DecoratorHandle) const;

		// Finds and returns the decorator associated with the provided decorator UID. If the decorator
		// is registered, nullptr is returned.
		const FDecorator* Find(FDecoratorUID DecoratorUID) const;

		// Registers a decorator dynamically
		void Register(FDecorator* Decorator);

		// Unregisters a decorator dynamically
		void Unregister(FDecorator* Decorator);

		// Returns the number of registered decorators
		uint32 GetNum() const;

	private:
		FDecoratorRegistry() = default;

		// Static init lifetime functions
		static void StaticRegister(DecoratorConstructorFunc DecoratorConstructor);
		static void StaticUnregister(DecoratorConstructorFunc DecoratorConstructor);
		void AutoRegisterImpl(DecoratorConstructorFunc DecoratorConstructor);
		void AutoUnregisterImpl(DecoratorConstructorFunc DecoratorConstructor);

		// Module lifetime functions
		static void Init();
		static void Destroy();

		// Holds information for each registered decorator
		struct FRegistryEntry
		{
			// A pointer to the decorator
			// TODO: Do we want a shared ptr here? we don't always own it
			FDecorator*					Decorator = nullptr;

			// A pointer to the constructor function
			// Only valid when the decorator has been auto-registered
			DecoratorConstructorFunc	DecoratorConstructor = nullptr;

			// The decorator handle
			FDecoratorRegistryHandle	DecoratorHandle;
		};

		// For performance reasons, we store static decorators that never unload into
		// a single contiguous memory buffer. However, decorators cannot be guaranteed
		// to be trivially copyable because they contain virtual functions. As such,
		// we cannot resize the buffer once they have been allocated. We reserve a fixed
		// amount of space that should easily cover our needs. Static decorators are
		// generally stateless and only contain a few v-tables. Their size is usually
		// less than 32 bytes. Additionally, we will likely only ever load a few hundred
		// decorators. If we exceed the size of the buffer, additional decorators will
		// be treated as dynamic. Dynamic decorators are instead allocated on the heap.
		static constexpr uint32			STATIC_DECORATOR_BUFFER_SIZE = 8 * 1024;

		uint8							StaticDecoratorBuffer[STATIC_DECORATOR_BUFFER_SIZE] = { 0 };
		int32							StaticDecoratorBufferOffset = 0;

		TArray<uintptr_t>				DynamicDecorators;
		int32							DynamicDecoratorFreeIndexHead = INDEX_NONE;

		TMap<uint32, FRegistryEntry>	DecoratorUIDToEntryMap;

		friend class FModule;
		friend struct FDecoratorStaticInitHook;
	};
}
