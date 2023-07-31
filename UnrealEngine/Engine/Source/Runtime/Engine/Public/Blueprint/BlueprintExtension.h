// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "UObject/Object.h"

#include "BlueprintExtension.generated.h"

class UBlueprint;
class FKismetCompilerContext;

/**
 * Per-instance extension object that can be added to UBlueprint::Extensions in order to augment built-in blueprint functionality
 * Ideally this would be an editor-only class, but such classes are not permitted within Engine modules (even inside WITH_EDITORONLY_DATA blocks)
 */
UCLASS()
class ENGINE_API UBlueprintExtension : public UObject
{
public:

	GENERATED_BODY()

#if WITH_EDITORONLY_DATA

	/**
	 * Called during compilation (after skeleton class generation) in order to generate dynamic function graphs for this blueprint
	 */
	void GenerateFunctionGraphs(FKismetCompilerContext* CompilerContext)
	{
		HandleGenerateFunctionGraphs(CompilerContext);
	}

	/**
	 * Called before blueprint compilation to ensure that any objects necessary for the specified blueprint's compilation
	 */
	void PreloadObjectsForCompilation(UBlueprint* OwningBlueprint)
	{
		HandlePreloadObjectsForCompilation(OwningBlueprint);
	}

private:

	/**
	 * Override this function to define custom preload logic into the specified blueprint
	 */
	virtual void HandlePreloadObjectsForCompilation(UBlueprint* OwningBlueprint) {}

	/**
	 * Override this function to define custom function generation logic for the specified blueprint compiler context
	 */
	virtual void HandleGenerateFunctionGraphs(FKismetCompilerContext* CompilerContext) {}

#endif // WITH_EDITORONLY_DATA
};
