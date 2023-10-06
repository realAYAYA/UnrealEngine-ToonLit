// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IAnimBlueprintCompilerHandler.h"

class FAnimBlueprintCompilerContext;
class IAnimBlueprintCompilerCreationContext;

class UE_DEPRECATED(5.0, "IAnimBlueprintCompilerHandlerCollection is no longer used") IAnimBlueprintCompilerHandlerCollection;

class ANIMGRAPH_API IAnimBlueprintCompilerHandlerCollection
{
public:
	PRAGMA_DISABLE_DEPRECATION_WARNINGS
	virtual ~IAnimBlueprintCompilerHandlerCollection() {}

	/** Register a named handler */
	static void RegisterHandler(FName InName, TFunction<TUniquePtr<IAnimBlueprintCompilerHandler>(IAnimBlueprintCompilerCreationContext&)> InFunction) {}

	/** Register a named handler */
	static void UnregisterHandler(FName InName) {}

	// Get a GetHandler of specified type with the specified name
	template<typename THandlerClass>
	THandlerClass* GetHandler(FName InName) const
	{
		return static_cast<THandlerClass*>(GetHandlerByName(InName));
	}

protected:
	// Get a GetHandler with the specified name
	virtual IAnimBlueprintCompilerHandler* GetHandlerByName(FName InName) const = 0;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
};