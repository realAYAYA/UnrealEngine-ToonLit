// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Templates/EnableIf.h"
#include "Templates/UnrealTypeTraits.h"


/** Struct that allows batching of transforms and custom data of multiple (possibly instanced) static mesh components */
struct FISMComponentBatcher
{
public:
	/**
	 * Add a single component to be batched
	 * @param	InActorComponent	Component to be batched
	 */
	ENGINE_API void Add(const UActorComponent* InActorComponent);

	/**
	 * Add a single component to be batched
	 * @param	InActorComponent	Component to be batched
	 * @param	InTransformFunc		Function that takes the world space transform of an instance and modifies it. Must return a world space transform.
	 */
	ENGINE_API void Add(const UActorComponent* InActorComponent, TFunctionRef<FTransform(const FTransform&)> InTransformFunc);
	
	/**
	 * Add an array of component to be batched
	 * @param	InComponents		Components to be batched
	 */
	template<typename TComponentClass = UActorComponent, typename = typename TEnableIf<TIsDerivedFrom<TComponentClass, UActorComponent>::IsDerived>::Type>
	void Append(const TArray<TComponentClass*>& InComponents)
	{
		for (const UActorComponent* InComponent : InComponents)
		{
			AddInternal(InComponent, TOptional<TFunctionRef<FTransform(const FTransform&)>>());
		}
	}

	/**
	 * Add an array of components to be batched
	 * @param	InComponents		Components to be batched
	 * @param	InTransformFunc		Function that takes the world space transform of an instance and modifies it. Must return a world space transform.
	 */
	template<typename TComponentClass = UActorComponent, typename = typename TEnableIf<TIsDerivedFrom<TComponentClass, UActorComponent>::IsDerived>::Type>
	void Append(const TArray<TComponentClass*>& InComponents, TFunctionRef<FTransform(const FTransform&)> InTransformFunc)
	{
		for (const UActorComponent* InComponent : InComponents)
		{
			AddInternal(InComponent, InTransformFunc);
		}
	}

	/**
	 * Return the number of instances batched so far.
	 */
	int32 GetNumInstances() const { return NumInstances; }

	/**
	 * Initialize the instances of the provided ISM component using the batched data stored in this class.
	 * @param	ISMComponent	Instanced static mesh component which will be modified.
	 */
	ENGINE_API void InitComponent(UInstancedStaticMeshComponent* ISMComponent) const;

	
	inline uint32 GetHash() const
	{
		if (Hash == 0)
		{
			ComputeHash();
		}
		return Hash;
	}

private:
	void ENGINE_API AddInternal(const UActorComponent* InComponent, TOptional<TFunctionRef<FTransform(const FTransform&)>> InTransformFunc);
	void ENGINE_API ComputeHash() const;

	mutable uint32 Hash = 0;
	int32 NumInstances = 0;
	int32 NumCustomDataFloats = 0;

	TArray<FTransform> InstancesTransformsWS;
	TArray<float> InstancesCustomData;
	TArray<FInstancedStaticMeshRandomSeed> RandomSeeds;
};
