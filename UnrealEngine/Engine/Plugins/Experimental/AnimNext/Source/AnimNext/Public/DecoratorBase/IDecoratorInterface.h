// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorBinding.h"			// Derived types will need to implement the binding specialization
#include "DecoratorBase/DecoratorInterfaceUID.h"

// Helper macros
#define DECLARE_ANIM_DECORATOR_INTERFACE(InterfaceName, InterfaceNameHash) \
	/* Globally unique UID for this interface */ \
	static constexpr UE::AnimNext::FDecoratorInterfaceUID InterfaceUID = UE::AnimNext::FDecoratorInterfaceUID(InterfaceNameHash, TEXT(#InterfaceName)); \
	virtual UE::AnimNext::FDecoratorInterfaceUID GetInterfaceUID() const override { return InterfaceUID; }

namespace UE::AnimNext
{
	struct FExecutionContext;	// Derived types will have functions that accept the execution context

	/**
	 * IDecoratorInterface
	 * 
	 * Base type for all decorator interfaces. Used for type safety.
	 */
	struct ANIMNEXT_API IDecoratorInterface
	{
		virtual ~IDecoratorInterface() {}

		// The globally unique UID for this interface
		// Derived types will have their own InterfaceUID member that hides/aliases/shadows this one
		// @see DECLARE_ANIM_DECORATOR_INTERFACE
		static constexpr FDecoratorInterfaceUID InterfaceUID = FDecoratorInterfaceUID(0x402f6df4, TEXT("IDecoratorInterface"));

		// Returns the globally unique UID for this interface
		virtual FDecoratorInterfaceUID GetInterfaceUID() const { return InterfaceUID; };
	};
}
