// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorUID.h"

namespace UE::AnimNext
{
	struct FNodeTemplate;

	/**
	  * FNodeTemplateBuilder
	  * 
	  * Utility to help construct node templates.
	  */
	struct ANIMNEXT_API FNodeTemplateBuilder
	{
		FNodeTemplateBuilder() = default;

		// Adds the specified decorator type to the node template
		void AddDecorator(FDecoratorUID DecoratorUID);

		// Returns a node template for the provided list of decorators
		// The node template will be built into the provided buffer and a pointer to it is returned
		FNodeTemplate* BuildNodeTemplate(TArray<uint8>& NodeTemplateBuffer) const;

		// Returns a node template for the provided list of decorators
		// The node template will be built into the provided buffer and a pointer to it is returned
		static FNodeTemplate* BuildNodeTemplate(const TArray<FDecoratorUID>& InDecoratorUIDs, TArray<uint8>& NodeTemplateBuffer);

		// Resets the node template builder
		void Reset();

	private:
		static uint32 GetNodeTemplateUID(const TArray<FDecoratorUID>& InDecoratorUIDs);
		static void AppendTemplateDecorator(
			const TArray<FDecoratorUID>& InDecoratorUIDs, int32 DecoratorIndex,
			TArray<uint8>& NodeTemplateBuffer);

		TArray<FDecoratorUID> DecoratorUIDs;	// The list of decorators to use when building the node template
	};
}
