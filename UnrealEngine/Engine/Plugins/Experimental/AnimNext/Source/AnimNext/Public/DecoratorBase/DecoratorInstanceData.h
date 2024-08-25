// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorPtr.h"

struct FAnimNextDecoratorSharedData;

namespace UE::AnimNext
{
	struct FDecoratorBinding;
	struct FExecutionContext;

	/**
	 * FDecoratorInstanceData
	 * A decorator instance represents allocated data for specific decorator instance.
	 * 
	 * @see FNodeInstance
	 * 
	 * A FDecoratorInstanceData is the base type that decorator instance data derives from.
	 */
	struct FDecoratorInstanceData
	{
		// Called after the constructor has been called when a new instance is created.
		// This is called after the default constructor.
		// You can override this function by adding a new one with the same name on your
		// derived type.
		// Decorators are constructed from the bottom to the top.
		void Construct(const FExecutionContext& Context, const FDecoratorBinding& Binding) noexcept
		{
		}

		// Called before the destructor has been called when an instance is destroyed.
		// This is called before the default destructor.
		// You can override this function by adding a new one with the same name on your
		// derived type.
		// Decorators are destructed from the top to the bottom.
		void Destruct(const FExecutionContext& Context, const FDecoratorBinding& Binding) noexcept
		{
		}
	};
}
