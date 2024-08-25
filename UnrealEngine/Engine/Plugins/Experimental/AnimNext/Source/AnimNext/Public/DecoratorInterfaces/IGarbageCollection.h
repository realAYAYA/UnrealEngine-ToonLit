// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/IDecoratorInterface.h"

class FReferenceCollector;

namespace UE::AnimNext
{
	/**
	 * IGarbageCollection
	 * 
	 * This interface exposes garbage collection reference tracking.
	 */
	struct ANIMNEXT_API IGarbageCollection : IDecoratorInterface
	{
		DECLARE_ANIM_DECORATOR_INTERFACE(IGarbageCollection, 0x231a2017)

		// Registers the provided binding for GC callback
		// Once registered, AddReferencedObjects is called during GC to collect references
		static void RegisterWithGC(const FExecutionContext& Context, const FDecoratorBinding& Binding);

		// Unregisters the provided binding from GC callback
		static void UnregisterWithGC(const FExecutionContext& Context, const FDecoratorBinding& Binding);

		// Called when garbage collection requests hard/strong object references
		// @see UObject::AddReferencedObjects
		virtual void AddReferencedObjects(const FExecutionContext& Context, const TDecoratorBinding<IGarbageCollection>& Binding, FReferenceCollector& Collector) const;
	};

	/**
	 * Specialization for decorator binding.
	 */
	template<>
	struct TDecoratorBinding<IGarbageCollection> : FDecoratorBinding
	{
		// @see IGarbageCollection::AddReferencedObjects
		void AddReferencedObjects(const FExecutionContext& Context, FReferenceCollector& Collector) const
		{
			GetInterface()->AddReferencedObjects(Context, *this, Collector);
		}

	protected:
		const IGarbageCollection* GetInterface() const { return GetInterfaceTyped<IGarbageCollection>(); }
	};
}
