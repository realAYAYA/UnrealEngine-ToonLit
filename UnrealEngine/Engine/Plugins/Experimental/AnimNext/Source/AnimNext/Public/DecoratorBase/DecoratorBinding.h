// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorPtr.h"
#include "DecoratorBase/DecoratorInterfaceUID.h"

struct FAnimNextDecoratorSharedData;

namespace UE::AnimNext
{
	struct FDecoratorInstanceData;
	struct IDecoratorInterface;

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

		// Returns whether or not this binding is valid and set to an interface.
		bool IsValid() const { return Interface != nullptr; }

		// Queries a node for a pointer to its decorator shared data.
		// If the decorator handle is invalid, a null pointer is returned.
		template<class SharedDataType>
		const SharedDataType* GetSharedData() const { return static_cast<const SharedDataType*>(SharedData); }

		// Queries a node for a pointer to its decorator instance data.
		// If the decorator handle is invalid, a null pointer is returned.
		template<class InstanceDataType>
		InstanceDataType* GetInstanceData() const { return static_cast<InstanceDataType*>(InstanceData); }

		// Returns the decorator pointer we are bound to.
		FWeakDecoratorPtr GetDecoratorPtr() const { return DecoratorPtr; }

		// Returns the decorator interface UID when bound, an invalid UID otherwise.
		FDecoratorInterfaceUID GetInterfaceUID() const;

		// Equality and inequality tests
		bool operator==(const FDecoratorBinding& RHS) const { return DecoratorPtr == RHS.DecoratorPtr && Interface == RHS.Interface; }
		bool operator!=(const FDecoratorBinding& RHS) const { return DecoratorPtr != RHS.DecoratorPtr || Interface != RHS.Interface; }

	protected:
		// Creates a valid binding
		FDecoratorBinding(const IDecoratorInterface* Interface_, const FAnimNextDecoratorSharedData* SharedData_, FDecoratorInstanceData* InstanceData_, FWeakDecoratorPtr DecoratorPtr_)
			: Interface(Interface_)
			, SharedData(SharedData_)
			, InstanceData(InstanceData_)
			, DecoratorPtr(DecoratorPtr_)
		{}

		// Performs a naked cast to the desired interface type
		template<class DecoratorInterfaceType>
		const DecoratorInterfaceType* GetInterfaceTyped() const { return static_cast<const DecoratorInterfaceType*>(Interface); }

		const IDecoratorInterface*		Interface = nullptr;

		const FAnimNextDecoratorSharedData*		SharedData = nullptr;
		FDecoratorInstanceData*			InstanceData = nullptr;

		FWeakDecoratorPtr				DecoratorPtr;

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
