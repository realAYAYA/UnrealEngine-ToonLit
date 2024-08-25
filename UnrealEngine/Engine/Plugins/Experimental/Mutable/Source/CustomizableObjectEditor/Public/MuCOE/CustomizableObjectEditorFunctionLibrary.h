// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "MuCO/CustomizableObject.h"

#include "CustomizableObjectEditorFunctionLibrary.generated.h"

// This mirrors the logic in CustomizableObjectEditor.cpp
UENUM(BlueprintType)
enum class ECustomizableObjectOptimizationLevel : uint8
{
	None,
	Minimal,
	Maximum
};

/**
 * Functions we want to be able to call on CustomizableObjects at edit time - could
 * be exposed to cook as well.
 */
UCLASS()
class UCustomizableObjectEditorFunctionLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** 
	 *	Synchronously compiles the provided CustomizableObject, LogMutable will contain intermittent updates on
	 *	progress.
	 * 
	 * @param CustomizableObject	The CustomizableObject to compile
	 * 
	 * @return	The final ECustomizableObjectCompilationState - typically Completed or Failed
	 */
	UFUNCTION(BlueprintCallable, Category = "CustomizableObject")
	static ECustomizableObjectCompilationState CompileCustomizableObjectSynchronously(
		UCustomizableObject* CustomizableObject, 
		ECustomizableObjectOptimizationLevel OptimizationLevel = ECustomizableObjectOptimizationLevel::Minimal, 
		ECustomizableObjectTextureCompression TextureCompression = ECustomizableObjectTextureCompression::Fast);
};
