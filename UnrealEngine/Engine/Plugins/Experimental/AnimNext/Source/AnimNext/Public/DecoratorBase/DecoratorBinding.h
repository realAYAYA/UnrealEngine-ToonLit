// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/DecoratorInterfaceUID.h"
#include "DecoratorBase/DecoratorTemplate.h"

#include <type_traits>

struct FAnimNextDecoratorSharedData;

namespace UE::AnimNext
{
	struct FDecoratorInstanceData;
	struct IDecoratorInterface;
	struct FNodeDescription;

	/**
	 * FDecoratorBinding
	 * 
	 * Base class for all decorator bindings.
	 * A decorator binding contains untyped data about a specific decorator instance.
	 */
	struct ANIMNEXT_API FDecoratorBinding
	{
		// Creates an empty binding.
		FDecoratorBinding() = default;

		// Returns whether or not this binding is valid.
		bool IsValid() const { return DecoratorTemplate != nullptr; }

		// Queries a node for a pointer to its decorator shared data.
		// If the decorator handle is invalid, a null pointer is returned.
		template<class SharedDataType>
		const SharedDataType* GetSharedData() const
		{
			static_assert(std::is_base_of<FAnimNextDecoratorSharedData, SharedDataType>::value, "Decorator shared data must derive from FAnimNextDecoratorSharedData");

			if (!IsValid())
			{
				return nullptr;
			}

			return static_cast<const SharedDataType*>(DecoratorTemplate->GetDecoratorDescription(*NodeDescription));
		}

		// Queries a node for a pointer to its decorator instance data.
		// If the decorator handle is invalid, a null pointer is returned.
		template<class InstanceDataType>
		InstanceDataType* GetInstanceData() const
		{
			static_assert(std::is_base_of<FDecoratorInstanceData, InstanceDataType>::value, "Decorator instance data must derive from FDecoratorInstanceData");

			if (!IsValid())
			{
				return nullptr;
			}

			return static_cast<InstanceDataType*>(DecoratorTemplate->GetDecoratorInstance(*DecoratorPtr.GetNodeInstance()));
		}

		// Queries a node for a pointer to its decorator latent properties.
		// If the decorator handle is invalid or if we have no latent properties, a null pointer is returned.
		const FLatentPropertyHandle* GetLatentPropertyHandles() const
		{
			if (!IsValid() || !DecoratorTemplate->HasLatentProperties())
			{
				return nullptr;
			}

			return DecoratorTemplate->GetDecoratorLatentPropertyHandles(*NodeDescription);
		}

		// Returns a pointer to the latent property specified by the provided handle or nullptr if the binding/handle are invalid
		template<typename PropertyType>
		const PropertyType* GetLatentProperty(FLatentPropertyHandle Handle) const
		{
			if (!IsValid())
			{
				return nullptr;
			}

			if (!Handle.IsOffsetValid())
			{
				return nullptr;
			}

			const uint8* NodeInstance = (const uint8*)DecoratorPtr.GetNodeInstance();
			return (const PropertyType*)(NodeInstance + Handle.GetLatentPropertyOffset());
		}

		// Returns the decorator pointer we are bound to.
		FWeakDecoratorPtr GetDecoratorPtr() const { return DecoratorPtr; }

		// Returns the decorator interface UID when bound, an invalid UID otherwise.
		FDecoratorInterfaceUID GetInterfaceUID() const;

		// Equality and inequality tests
		bool operator==(const FDecoratorBinding& RHS) const { return DecoratorPtr == RHS.DecoratorPtr && Interface == RHS.Interface; }
		bool operator!=(const FDecoratorBinding& RHS) const { return DecoratorPtr != RHS.DecoratorPtr || Interface != RHS.Interface; }

	protected:
		// Creates a valid binding
		FDecoratorBinding(const IDecoratorInterface* InInterface, const FDecoratorTemplate* InDecoratorTemplate, const FNodeDescription* InNodeDescription, FWeakDecoratorPtr InDecoratorPtr)
			: Interface(InInterface)
			, DecoratorTemplate(InDecoratorTemplate)
			, NodeDescription(InNodeDescription)
			, DecoratorPtr(InDecoratorPtr)
		{}

		// Performs a naked cast to the desired interface type
		template<class DecoratorInterfaceType>
		const DecoratorInterfaceType* GetInterfaceTyped() const
		{
			static_assert(std::is_base_of<IDecoratorInterface, DecoratorInterfaceType>::value, "Decorator interface data must derive from IDecoratorInterface");
			return static_cast<const DecoratorInterfaceType*>(Interface);
		}

		// A pointer to the bound interface or nullptr if we are bound to a decorator but none of its interfaces
		const IDecoratorInterface*				Interface = nullptr;

		// A pointer to the decorator template that implements the interface we are bound to or nullptr if we are invalid
		const FDecoratorTemplate*				DecoratorTemplate = nullptr;

		// A pointer to the node shared data we are bound to
		const FNodeDescription*					NodeDescription = nullptr;

		// A weak handle to the decorator instance data we are bound to
		FWeakDecoratorPtr						DecoratorPtr;

		friend struct FExecutionContext;
	};

	/**
	 * TDecoratorBinding
	 * 
	 * A templated proxy for decorator interfaces. It is meant to be specialized per interface
	 * in order to allow a clean API and avoid human error. It wraps the necessary information
	 * to bind a decorator to a specific interface. See existing interfaces for examples.
	 * 
	 * Here, we forward declare the template which every interface must specialize. Because we
	 * rely on specialization, it must be defined within the UE::AnimNext namespace where the
	 * declaration exists.
	 * 
	 * Specializations must derive from FDecoratorBinding to provide the necessary machinery.
	 * 
	 * @see IUpdate, IEvaluate, IHierarchy
	 */
	template<class DecoratorInterfaceType>
	struct TDecoratorBinding;
}
