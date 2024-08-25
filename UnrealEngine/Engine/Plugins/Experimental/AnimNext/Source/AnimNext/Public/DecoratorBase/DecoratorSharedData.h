// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "DecoratorBase/DecoratorBinding.h"
#include "DecoratorBase/ExecutionContext.h"
#include "DecoratorBase/LatentPropertyHandle.h"

#include <type_traits>

#include "DecoratorSharedData.generated.h"

#define ANIM_NEXT_IMPL_GET_LATENT_PROPERTY_INDEX_FOR_PROPERTY(PropertyName) \
	LatentPropertyIndex--; \
	if (LatentPropertyOffset == offsetof(Self, PropertyName)) \
	{ \
		return -LatentPropertyIndex; \
	} \

#define ANIM_NEXT_IMPL_DEFINE_GET_LATENT_PROPERTY_INDEX(EnumeratorMacro) \
	/** @see FAnimNextDecoratorSharedData::GetLatentPropertyIndex */ \
	static constexpr int32 GetLatentPropertyIndex(const size_t LatentPropertyOffset) \
	{ \
		int32 LatentPropertyIndex = Super::GetLatentPropertyIndex(LatentPropertyOffset); \
		/* If the value is positive, the property lives in our base type */ \
		/* Otherwise the value is the negative (or zero) number of latent properties in the base type */ \
		if (LatentPropertyIndex > 0) \
		{ \
			return LatentPropertyIndex; \
		} \
		/* If a property in the struct is wrapped with WITH_EDITORONLY_DATA, then this code here */ \
		/* needs to be wrapped as well */ \
		EnumeratorMacro(ANIM_NEXT_IMPL_GET_LATENT_PROPERTY_INDEX_FOR_PROPERTY) \
		/* Latent property wasn't found, return the number of latent properties seen so far */ \
		return LatentPropertyIndex; \
	} \

#define ANIM_NEXT_IMPL_DEFINE_LATENT_GETTER(PropertyName) \
	decltype(PropertyName) Get##PropertyName(const UE::AnimNext::FExecutionContext& Context, const UE::AnimNext::FDecoratorBinding& Binding) const \
	{ \
		/* We need a mapping of latent property name/offset to latent property index */ \
		/* This can be built once at runtime using the UE reflection and cached on first call or using a constexpr function, see below */ \
		constexpr size_t LatentPropertyOffset = offsetof(Self, PropertyName); \
		constexpr int32 LatentPropertyIndex = GetLatentPropertyIndex(LatentPropertyOffset); \
		/* A negative or zero property index means the property isn't latent, we can static assert since it isn't possible */ \
		static_assert(LatentPropertyIndex > 0, "Property " #PropertyName " isn't latent"); \
		const UE::AnimNext::FLatentPropertyHandle* LatentPropertyHandles = Binding.GetLatentPropertyHandles(); \
		const UE::AnimNext::FLatentPropertyHandle LatentPropertyHandle = LatentPropertyHandles[LatentPropertyIndex - 1]; \
		/* An invalid offset means we are inline, otherwise we are cached */ \
		if (!LatentPropertyHandle.IsOffsetValid()) \
		{ \
			return PropertyName; \
		} \
		else \
		{ \
			return *Binding.GetLatentProperty<decltype(PropertyName)>(LatentPropertyHandle); \
		} \
	} \

/**
  * This macro defines the necessary boilerplate for latent property support.
  * It takes as arguments the shared data struct name that owns the latent properties
  * and an enumerator macro. The enumerator macro should accept one argument which will
  * be a macro to apply to each latent property. We then use it to define what we need.
  */
#define GENERATE_DECORATOR_LATENT_PROPERTIES(SharedDataType, EnumeratorMacro) \
	using Self = SharedDataType; \
	ANIM_NEXT_IMPL_DEFINE_GET_LATENT_PROPERTY_INDEX(EnumeratorMacro) \
	EnumeratorMacro(ANIM_NEXT_IMPL_DEFINE_LATENT_GETTER) \

/**
 * FAnimNextDecoratorSharedData
 * Decorator shared data represents a unique instance in the authored static graph.
 * Each instance of a graph will share instances of the read-only shared data.
 * Shared data typically contains hardcoded properties, RigVM latent pin information,
 * or pooled properties shared between multiple decorators.
 * 
 * @see FNodeDescription
 *
 * A FAnimNextDecoratorSharedData is the base type that decorator shared data derives from.
 */
USTRUCT()
struct FAnimNextDecoratorSharedData
{
	GENERATED_BODY()

	/**
	  * Returns the latent property index from a latent property offset
	  * If the property is found within this type, its latent property 1-based index is returned (a positive value)
	  * If the property isn't found, this function returns the number of latent properties (as a zero or negative value)
	  * Derived types will have latent property indices higher than their base type
	  */
	static constexpr int32 GetLatentPropertyIndex(const size_t LatentPropertyOffset) { return 0; }
};
