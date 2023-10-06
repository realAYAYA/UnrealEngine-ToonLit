// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorMode.h"
#include "DecoratorBase/DecoratorUID.h"
#include "DecoratorBase/DecoratorRegistryHandle.h"

#include <type_traits>

class FArchive;
struct FAnimNextDecoratorSharedData;

namespace UE::AnimNext
{
	struct FDecoratorInstanceData;
	struct FNodeDescription;
	struct FNodeInstance;

	/**
	 * Decorator Template
	 * A decorator template represents a specific decorator within a FNodeTemplate.
	 * 
	 * @see FNodeTemplate
	 */
	struct FDecoratorTemplate
	{
		FDecoratorTemplate(FDecoratorUID UID_, FDecoratorRegistryHandle RegistryHandle_, EDecoratorMode Mode_, uint32 AdditiveIndexOrNumAdditive_, uint32 NodeSharedOffset_, uint32 NodeInstanceOffset_)
			: UID(UID_.GetUID())
			, RegistryHandle(RegistryHandle_)
			, Mode(static_cast<uint8>(Mode_))
			, AdditiveIndexOrNumAdditive(AdditiveIndexOrNumAdditive_)
			, NodeSharedOffset(NodeSharedOffset_)
			, NodeInstanceOffset(NodeInstanceOffset_)
		{}

		// Returns the globally unique identifier for the decorator
		uint32 GetUID() const { return UID; }

		// Returns the decorator registry handle
		FDecoratorRegistryHandle GetRegistryHandle() const { return RegistryHandle; }

		// Returns the decorator mode
		EDecoratorMode GetMode() const { return static_cast<EDecoratorMode>(Mode); }

		// For base decorators only, returns the number of additive decorators on top in the stack
		uint32 GetNumAdditiveDecorators() const
		{
			check(GetMode() == EDecoratorMode::Base);
			return AdditiveIndexOrNumAdditive;
		}

		// For additive decorators only, returns the decorator index relative to the base decorator on the stack
		uint32 GetAdditiveDecoratorIndex() const
		{
			check(GetMode() == EDecoratorMode::Additive);
			return AdditiveIndexOrNumAdditive;
		}

		// Returns the offset into the shared data where the descriptor begins, relative to the root of the node's shared data
		uint32 GetNodeSharedOffset() const { return NodeSharedOffset; }

		// Returns the offset into the instance data where the descriptor begins, relative to the root of the node's instance data
		uint32 GetNodeInstanceOffset() const { return NodeInstanceOffset; }

		// Returns a pointer to the specified decorator description on the current node
		FAnimNextDecoratorSharedData* GetDecoratorDescription(FNodeDescription& NodeDescription) const
		{
			return reinterpret_cast<FAnimNextDecoratorSharedData*>(reinterpret_cast<uint8*>(&NodeDescription) + GetNodeSharedOffset());
		}

		// Returns a pointer to the specified decorator description on the current node
		const FAnimNextDecoratorSharedData* GetDecoratorDescription(const FNodeDescription& NodeDescription) const
		{
			return reinterpret_cast<const FAnimNextDecoratorSharedData*>(reinterpret_cast<const uint8*>(&NodeDescription) + GetNodeSharedOffset());
		}

		// Returns a pointer to the specified decorator instance on the current node
		FDecoratorInstanceData* GetDecoratorInstance(FNodeInstance& NodeInstance) const
		{
			return reinterpret_cast<FDecoratorInstanceData*>(reinterpret_cast<uint8*>(&NodeInstance) + GetNodeInstanceOffset());
		}

		// Returns a pointer to the specified decorator instance on the current node
		const FDecoratorInstanceData* GetDecoratorInstance(const FNodeInstance& NodeInstance) const
		{
			return reinterpret_cast<const FDecoratorInstanceData*>(reinterpret_cast<const uint8*>(&NodeInstance) + GetNodeInstanceOffset());
		}

		// Serializes this decorator template instance
		ANIMNEXT_API void Serialize(FArchive& Ar);

	private:
		uint32	UID;							// decorator globally unique identifier

		FDecoratorRegistryHandle	RegistryHandle;	// decorator registry handle
		uint8						Mode;			// the decorator mode (we only need 1 bit)

		// For base decorators, this contains the number of additive decorators on top
		// For additive decorators, this contains its index relative to the base decorator
		// The first additive decorator has index 1, there is no index 0 (we re-purpose it for the base decorator)
		uint8	AdditiveIndexOrNumAdditive;

		// Offsets into the shared read-only and instance data portions of a node
		uint16	NodeSharedOffset;				// relative to root of node description data (max 64 KB per node)
		uint16	NodeInstanceOffset;				// relative to root of node instance data (max 64 KB per node)

		// TODO: We could cache which parent/base decorator handles which interface
		// This would avoid the need to iterate on every decorator to look up a 'Super'. Perhaps only common interfaces could be cached.
	};

	static_assert(std::is_trivially_copyable_v<FDecoratorTemplate>, "FDecoratorTemplate needs to be trivially copyable");
}
