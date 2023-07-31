// Copyright Epic Games, Inc. All Rights Reserved.

/**
 * This UV node generates texture coordinates in view space centered on the particle system's MacroUVPosition, with tiling controlled by the particle system's MacroUVRadius.
 * It is useful for mapping a 'macro' noise texture in a continuous manner onto all particles of a particle system.
 */

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Materials/MaterialExpression.h"
#include "MaterialExpressionParticleMacroUV.generated.h"

UCLASS()
class UMaterialExpressionParticleMacroUV : public UMaterialExpression
{
	GENERATED_UCLASS_BODY()


	//~ Begin UMaterialExpression Interface
#if WITH_EDITOR
	virtual int32 Compile(class FMaterialCompiler* Compiler, int32 OutputIndex) override;
	virtual void GetCaption(TArray<FString>& OutCaptions) const override;
#endif
	//~ End UMaterialExpression Interface
};



