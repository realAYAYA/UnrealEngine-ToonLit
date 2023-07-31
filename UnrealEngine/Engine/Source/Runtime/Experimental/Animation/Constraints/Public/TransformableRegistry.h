// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Map.h"
#include "HAL/Platform.h"
#include "Templates/Function.h"

class UClass;
class UObject;
class UTransformableHandle;

/**
 * TransformableRegistry
 * Can be used to register customization transformable handle factory for specific objects. 
 */

class CONSTRAINTS_API FTransformableRegistry
{
public:
	/** @todo document */
	using CreateHandleFuncT = TFunction<UTransformableHandle*(const UObject*, UObject*)>;
	using GetHashFuncT = TFunction<uint32(const UObject*)>;
	
	~FTransformableRegistry();

	/** @todo document */
	static FTransformableRegistry& Get();

	/** @todo document */
	void Register(UClass* InClass, CreateHandleFuncT InHandleFunc, GetHashFuncT InHashFunc);

	/** @todo document */
	GetHashFuncT GetHashFunction(const UClass* InClass) const;
	/** @todo document */
	CreateHandleFuncT GetCreateFunction(const UClass* InClass) const;

private:
	FTransformableRegistry() = default;

	/** @todo document */
	struct FTransformableInfo
	{
		CreateHandleFuncT CreateHandleFunc;
		GetHashFuncT GetHashFunc;
	};

	/** @todo document */
	const FTransformableInfo* FindInfo(const UClass* InClass) const;

	/** @todo document */
	TMap<UClass*, FTransformableInfo> Transformables;
};
