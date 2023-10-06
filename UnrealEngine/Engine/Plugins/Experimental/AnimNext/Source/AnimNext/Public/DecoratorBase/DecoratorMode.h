// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace UE::AnimNext
{
	/**
	 * EDecoratorMode
	 *
	 * Describes how a decorator behaves once attached to an animation node.
	 */
	enum class EDecoratorMode
	{
		/**
		 * Base decorators can live on their own in an animation node and have no 'Super'.
		 * As a result, calls to GetInterface() do not forward to other decorators below
		 * them on the node stack. Multiple base decorators can exist in a single animation
		 * node but they behave as independent nodes (functionally speaking).
		 */
		Base,

		/**
		 * Additive decorators override or augment behavior on prior decorators on the node stack.
		 * At least one base decorator must be present for an additive decorator to be added
		 * on top. Calls to GetInterface() will pass-through to other decorators below on the stack
		 * until a base decorator is found.
		 */
		Additive,
	};
}
